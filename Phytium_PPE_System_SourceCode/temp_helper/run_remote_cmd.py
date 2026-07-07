import paramiko
import sys

def run_cmd(cmd):
    host = "172.20.10.2"
    port = 22
    username = "user"
    password = "user"
    
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    try:
        ssh.connect(host, port=port, username=username, password=password)
        stdin, stdout, stderr = ssh.exec_command(cmd)
        out = stdout.read().decode('utf-8', errors='ignore')
        err = stderr.read().decode('utf-8', errors='ignore')
        if out:
            print("=== STDOUT ===")
            print(out.encode(sys.stdout.encoding, errors='ignore').decode(sys.stdout.encoding))
        if err:
            print("=== STDERR ===")
            print(err.encode(sys.stderr.encoding, errors='ignore').decode(sys.stderr.encoding))
        ssh.close()
    except Exception as e:
        print("Error:", e)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        run_cmd(sys.argv[1])
    else:
        print("Usage: python run_remote_cmd.py <command>")
