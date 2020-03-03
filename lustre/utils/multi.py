import subprocess

def exceeding_quota_set(fsname, uids):
    proc = subprocess.Popen(['./test_multi_quota', fsname],
                        stdin=subprocess.PIPE,
                        stdout=subprocess.PIPE,)

    # pack all the uids into bytes like a C string
    uid_string = ('\n'.join([str(x) for x in uids])).encode()
    out = proc.communicate(input=uid_string)[0]
    return {uid for uid,offending in zip(uids, out) if offending}


