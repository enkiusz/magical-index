#!/usr/bin/env python3

import argparse, sys, logging, os, re, subprocess

logging.basicConfig(level=logging.INFO)
log = logging.getLogger(__name__)

def read_vmx(vmx_filename):
    f = open(vmx_filename, "r")
    vmx = {}
    for line in f.readlines():
        line = line.rstrip()
        if len(line) == 0:
            continue

        m = re.match(r"(?P<key>\S+)\s*=\s*\"(?P<value>.*)\"", line)
        # print("PARSE: %s=%s" % (m.group("key"), m.group("value")) )
        vmx[m.group("key")] = m.group("value")

    f.close()
    return vmx

def write_vmx(vmx, vmx_filename):
    f = open(vmx_filename, "w")
    for k,v in vmx.items():
        f.write("%s = \"%s\"\n" % (k,v))
    f.close()

def die(msg):
    print(msg)
    sys.exit(1)

if __name__ == "__main__":

    parser = argparse.ArgumentParser(description='Clone VMware machine',
                                     epilog="Example: clone.py -t template/template.vmx instance1 instance2 instance3 will create instance[1-3] from template/template.vmx")
    parser.add_argument('-t', metavar='VMX', dest='template_vmx_filename', action='store', required=True,
                        help='set the template vmx file location')
    parser.add_argument('names', nargs=argparse.REMAINDER)

    args = parser.parse_args()

    template_vm_dir = os.path.dirname(args.template_vmx_filename)

    for tgt_name in args.names:
        log.info("Cloning '%s' into new VM '%s'" % (args.template_vmx_filename, tgt_name))

        tgt_dir = tgt_name
        os.makedirs(tgt_dir)

        tgt_vmx_filename = os.path.join(tgt_dir, tgt_name + '.vmx')

        log.debug("Storing new VMX into '%s'" % (tgt_vmx_filename))

        template_vmx = read_vmx(args.template_vmx_filename)

        tgt_vmx = dict()
        # Prepare a new VMX template
        #
        # Remove instance-specific keys:
        # Remove all pciSlot entries, these are automatically assigned
        # Remove the generated Ethernet addresses
        # Remove the displayName
        # Remove unique identifiers
        # Remove logs and swaps
        for k in template_vmx.keys():
            if not re.match(r'^.+\.pciSlotNumber|^ethernet[0-9]+\.generatedAddress|^displayName|^uuid\.bios|^uuid\.location|^vmci\d+\.id|^sched\.swap\.derivedName|^migrate\.hostLog', k):
                tgt_vmx[k] = template_vmx[k]

        # Reconfigure storage (clone VMDK files and so on)
        for k in tgt_vmx.keys():
            v = tgt_vmx[k]

            # Look for all SCSI devices (skip IDE devices as they most likely contain ISOs)
            if not re.match(r'^scsi\d+:\d+\.fileName', k):
                continue

            # Don't touch files which we cannot clone
            if not v.endswith('.vmdk'):
                log.warn("Copying reference to storage file '%s=%s' in '%s'" % (k,v, tgt_vmx_filename))
                continue

            vmdk_filename = v
            log.info("Cloning VMDK '%s'" % (vmdk_filename))
            os.system("vmkfstools -d thin -i %s %s" % (os.path.join(template_vm_dir, vmdk_filename), os.path.join(tgt_dir, vmdk_filename)) )

        # Set a new name
        tgt_vmx["displayName"] = tgt_name

        # Dump the new VMX configuration
        log.debug(template_vmx)

        # Write new configuration
        write_vmx(tgt_vmx, tgt_vmx_filename)

        # Register the new VM (and get the new ID)
        r = subprocess.run(["/bin/vim-cmd", 'solo/registervm', "%s" % (os.path.abspath(tgt_vmx_filename))], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if r.returncode != 0:
            log.warn("Could not create new VM from vmx '%s', exit code is '%d', stdout: '%s' stderr: '%s'" % (tgt_vmx_filename, r.returncode, r.stdout, r.stderr))

        tgt_id = int(r.stdout)
        log.info("Virtual machine from '%s' created as '%d'" % (tgt_vmx_filename, tgt_id))

        os.system("vim-cmd vmsvc/power.on %d" % (tgt_id))
