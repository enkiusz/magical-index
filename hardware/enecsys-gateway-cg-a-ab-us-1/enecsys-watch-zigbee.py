#!/usr/bin/env python

from re import I
import sys
import argparse
import logging
import structlog
import requests
from xml.dom.minidom import parseString
import base64
from struct import unpack, calcsize
from hexdump import hexdump
import fileinput
from datetime import timedelta
from binascii import hexlify, unhexlify

# Default logging configuration
structlog.configure(
    wrapper_class=structlog.make_filtering_bound_logger(logging.INFO),
    logger_factory=structlog.PrintLoggerFactory(file=sys.stderr)
)

# Reference: https://stackoverflow.com/a/49724281
LOG_LEVEL_NAMES = [logging.getLevelName(v) for v in
                   sorted(getattr(logging, '_levelToName', None) or logging._levelNames) if getattr(v, "real", 0)]


log = structlog.get_logger()

def format_eui64(b):
    b = list(b)
    b.reverse()
    return ':'.join([ f'{v:02x}' for v in b])


def consume(b, formatstring):
    datasize = calcsize(formatstring)

    try:
        unpacked = unpack(formatstring, b[0:datasize])
        b = b[datasize:]
    except Exception as e:
        log.error('cannot unpack', b=b, formatstring=formatstring, datasize=datasize)
        raise e

    return (b, *unpacked)


def parse_pkt(pkt):
    log.info('parsing packet', pkt=pkt)

    flavor = pkt[0:2]
    payload = pkt[3:]
    kvps = None
    try:
        comma = payload.index(',')
        kvps = payload[comma+1:]
        payload = payload[:comma]
    except ValueError:
        pass

    # Last two characters of the payload are probably some kind of checksum
    checksum = payload[-2:]
    payload = payload[:-2]

    try:
        # The payload is encoded with base64
        payload = base64.urlsafe_b64decode(payload)
    except Exception as e:
        log.error('cannot decode base64', pkt=pkt)
        return None

    log.info('unwrapped', flavor=flavor, kvps=kvps, checksum=checksum)

    print("PAYLOAD")
    hexdump(payload)

    #
    # Parse fields common to both WZ ans WS flavors
    #

    (payload, eui64_bytes) = consume(payload, '!8s')
    eui64 = format_eui64(eui64_bytes)

    (payload, uptime, type, counter, length) = consume(payload, '!IHIB')

    # The uptime unit is 500ms
    uptime = timedelta(seconds=uptime * 0.5)

    log.info('common fields', eui64=eui64, uptime=str(uptime), type=hex(type), counter=counter, length=length)

    (payload, contents) = consume(payload, f'{length}s')

    print("CONTENTS")
    hexdump(contents)

    if flavor == 'WZ':
        return  # We're not interested in WZ messages for now

        if type == 0x2100:  # Bootup message

            log.info('gw bootup', gw_eui64=eui64)

            # Hypothesis for contents:
            #
            # hardware version?
            # software version?
            # country?
            # manufacturing date?
            # configuration?
            if contents not in (
                    b'PR\x02',  # This is the first packet sent with the 0x2100 type
                    b'N\x12\x00d\xb5\xdc\x9e`{\xec\x19\xe1\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'  # This is the second packet sent
            ):
                log.warn('unexpected contents', pkt=pkt, contents=contents)

        elif type == 0x2101:  # Connectivity report, sent out reporting the EUI64 of seen inverters

            log.info('gw beacon', contents=contents)

            (contents, code1, device_eui64_bytes, code2) = consume(contents, 'B8sB')
            device_eui64 = format_eui64(device_eui64_bytes)

            log.info('gw device report', gw_eui64=eui64, device=device_eui64, code1=code1, code2=code2)

            if code1 != 0x53:
                log.warn('unexpected value', pkt=pkt, code1=code1)

            if code2 != 0x48:
                log.warn('unexpected value', pkt=pkt, code2=code2)

        else:
            log.warn('unknown message type', pkt=pkt, type=hex(type), contents=contents)


    elif flavor == 'WS':

        if type == 0x2100:  # Bootup message
            (contents, unknown1) = consume(contents, '7s')

            (contents, text1) = consume(contents, '16s')
            text1 = text1.decode('ascii').rstrip('\x00')

            (contents, unknown2) = consume(contents, '!H')
            (contents, unknown3) = consume(contents, '!I')

            (contents, text2) = consume(contents, '16s')

            log.info('inverter bootup', unknown1=unknown1, text1=text1, unknown2=unknown2, unknown3=unknown3, text2=text2)

            if unknown1 != b'r\x01\x03\x05\xf7\x05\xf5':  # Value seen so far
                log.warn('unexpected value', pkt=pkt, unknown1=unknown1)

            if text1 != 'WSI-00003':
                log.warn('unexpected value', pkt=pkt, text1=text1)

            if unknown2 != 0x003c:
                log.warn('unexpected value', pkt=pkt, unknown2=unknown2)

            if unknown3 != 0x0000:
                log.warn('unexpected value', pkt=pkt, unknown3=unknown3)


        elif type in (0x2101, 0x2102):
            if type == 0x2101:  # Measurements IDLE - sent when the inverter appears to be offline or idle
                t_label = 'inverter idle'
            elif type == 0x2102:  # Measurements ONLINE - send when the inverter appears to be online and generating power
                t_label = 'inverter online'

            (contents, unknown1, dc_power, ac_power, efficiency, ac_frequency, mppt_drive, unknown2, mppt_flag, unknown3) = consume(contents, '!4sHHHBxH4sBB')

            # efficiency:
            efficiency = 0.1 * efficiency

            # mppt_flag:
            # -> 04 means voltage is too low for MPPT
            # -> 00 means MPPT is operating
            v_ac = 0.004 * mppt_drive

            log.info(t_label, unknown1=unknown1, ac_frequency=ac_frequency, v_ac=round(v_ac,2), mppt_drive=mppt_drive, dc_power=dc_power, ac_power=ac_power, efficiency=efficiency, unknown2=unknown2, mppt_flag=mppt_flag, unknown3=unknown3)

        else:
            log.warn('unknown message type', pkt=pkt, type=hex(type), contents=contents)

    else:
        log.warning('unknown message flavor', pkt=pkt, flavor=flavor)
        return None

    if len(contents) > 0:
        print("UNPARSED CONTENTS")
        hexdump(contents)

    if len(payload) > 0:
        print("UNPARSED PAYLOAD")
        hexdump(payload)

def zigbee_packets(config):
    log.info('polling start', url=config.url)
    total_requests = 0

    while True:
        response = requests.get(config.url)
        response.raise_for_status()

        total_requests +=1
        if total_requests % 100 == 0:
            log.info('polling stats', total_requests=total_requests)

        dom = parseString(response.text)
        try:
            pkt = dom.getElementsByTagName('zigbeeData')[0].firstChild.nodeValue
            pkt = pkt.rstrip()

            if pkt == '??': # Skip when ETRX2 boots (or is some bootloader?)
                log.warn('bootloader handshake?', pkt=pkt)
                continue

            log.info('packet from Zigbee module', pkt=pkt)
            yield pkt
        except AttributeError as e:
            continue

    raise RuntimeError("We shouldn't have gotten here")


def file_packets(config):
    for pkt in fileinput.input(files=config.files, encoding='ascii'):
        pkt = pkt.strip()
        if len(pkt) == 0:  # skip empty lines
            continue

        if pkt[0] == '#':  # skip comments
            continue

        yield pkt


def main(config):

    if config.url:
        packet_source = zigbee_packets(config)
    elif config.files:
        packet_source = file_packets(config)

    for pkt in packet_source:
       parse_pkt(pkt)


if __name__ == "__main__":

    parser = argparse.ArgumentParser(description='Poll the Enecsys Gateway for Zigbee packets')
    parser.add_argument('--loglevel', choices=LOG_LEVEL_NAMES, default='INFO', help='Change log level')
    parser.add_argument('--url', help='Poll messages from gateway URL')
    parser.add_argument('--file', dest='files', nargs='+', help='Read messages from file, use - for standard input')

    (args) = parser.parse_args()
    config = args

    # Restrict log message to be above selected level
    structlog.configure( wrapper_class=structlog.make_filtering_bound_logger(getattr(logging, args.loglevel)) )

    log.debug('config', args=args)

    main(config)
