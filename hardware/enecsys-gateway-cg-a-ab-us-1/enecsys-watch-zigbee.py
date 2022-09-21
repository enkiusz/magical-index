#!/usr/bin/env python

from re import I
import sys
import argparse
import logging
import structlog
import requests
from xml.dom.minidom import parseString
import base64
from struct import unpack
from hexdump import hexdump

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

def parse_pkt(pkt):
    log.debug('parsing packet', pkt=pkt)

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

    # The payload is encoded with base64
    payload = base64.urlsafe_b64decode(payload)

    log.info('extracted fields', flavor=flavor, payload=payload, kvps=kvps, checksum=checksum)

    if flavor == 'WZ':
        print(pkt)
        hexdump(payload)
        (eui64_bytes, counter1, static1) = unpack('!8sI12x5s', payload)
        gw_eui64 = format_eui64(eui64_bytes)

        # static1 is always b'\x00\x9a\xc64H
        if static1 != b'\x00\x9a\xc64H':
            log.warn('static1 value is not typical', pkt=pkt, static1=static1)

        log.info('payload parsed', gw_eui64=gw_eui64, counter1=counter1, static1=static1)

    elif flavor == 'WS':
        print(pkt)
        hexdump(payload)
        (eui64_bytes, counter1) = unpack('!8sI27x', payload)
        inverter_eui64 = format_eui64(eui64_bytes)

        log.info('payload parsed', inverter_eui64=inverter_eui64, counter1=counter1)

    else:
        log.warning('unknown packet flavor', pkt=pkt, flavor=flavor)
        return None


def zigbee_packets(config):
    log.info('polling start', url=config.url)
    while True:
        response = requests.get(config.url)
        log.debug('response', response=response)
        response.raise_for_status()

        dom = parseString(response.text)
        try:
            pkt = dom.getElementsByTagName('zigbeeData')[0].firstChild.nodeValue
            pkt = pkt.rstrip()
            log.info('packet from Zigbee module', pkt=pkt)
            yield pkt
        except AttributeError as e:
            continue

    raise RuntimeError("We shouldn't have gotten here")

def main(config):

    for pkt in zigbee_packets(config):
       parse_pkt(pkt)

    #parse_pkt('WZ=qcE1dwCaxjQAAAZhIQEAAAAMClP1BfcFAJrGNEg=AE,S=2000011689')
    #parse_pkt('WS=9QX3BQCaxjQAAAbLIQEAAAAAFDADiAEAAgAAAAAAAAAYAY0AAwQF00')

if __name__ == "__main__":

    parser = argparse.ArgumentParser(description='Poll the Enecsys Gateway for Zigbee packets')
    parser.add_argument('--loglevel', choices=LOG_LEVEL_NAMES, default='INFO', help='Change log level')

    parser.add_argument('--url', help='The gateway URL')
    
    (args) = parser.parse_args()
    config = args

    # Restrict log message to be above selected level
    structlog.configure( wrapper_class=structlog.make_filtering_bound_logger(getattr(logging, args.loglevel)) )

    log.debug('config', args=args)

    main(config)