import subprocess
print "Building the detector module."
result = subprocess.Popen("make", stdout=subprocess.PIPE, shell=True)
result.wait()
ssres1 = subprocess.Popen("ss -t -a", stdout=subprocess.PIPE, shell=True)
ssres1 = ssres1.stdout.read()
print "Running the detector module. This is the output:"
result = subprocess.Popen("insmod mod.ko", stdout=subprocess.PIPE, shell=True)
result.wait()
print result.stdout.read()
ssres2 = subprocess.Popen("ss -t -a", stdout=subprocess.PIPE, shell=True)
ssres2 = ssres2.stdout.read()
subprocess.Popen("rmmod mod", stdout=subprocess.PIPE, shell=True)
result.wait()

ssres1 = ssres1.split("\n")
ssres2 = ssres2.split("\n")
if len(ssres1)!=len(ssres2):
    print "Warning: There may be a hidden TCP socket:"
    for l in [s for s in ssres2 if not (s in ssres1)]:
        print l
