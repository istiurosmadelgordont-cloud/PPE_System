import paramiko

def fix_remote_files():
    host = "172.20.10.2"
    username = "user"
    password = "user"
    
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    
    try:
        print(f"Connecting to {host}...")
        ssh.connect(host, username=username, password=password, timeout=10)
        print("Connected!")
        
        # We will use Python3 on the remote board to clean up \r (CR) characters.
        # This avoids all escaping and shell expansion bugs.
        cmd1 = "python3 -c \"f='/home/user/Phytium_PPE_System_SourceCode/ppe_system/run_real_deepseek.sh'; data=open(f,'r',encoding='utf-8').read().replace('\\r',''); open(f,'w',encoding='utf-8').write(data)\""
        stdin, stdout, stderr = ssh.exec_command(cmd1)
        err1 = stderr.read().decode()
        if err1:
            print("Error fixing run_real_deepseek.sh:", err1)
        else:
            print("Successfully fixed run_real_deepseek.sh line endings remotely!")

        cmd2 = "python3 -c \"f='/home/user/Phytium_PPE_System_SourceCode/ppe_system/scripts/feishu_push.py'; data=open(f,'r',encoding='utf-8').read().replace('\\r',''); open(f,'w',encoding='utf-8').write(data)\""
        stdin, stdout, stderr = ssh.exec_command(cmd2)
        err2 = stderr.read().decode()
        if err2:
            print("Error fixing feishu_push.py:", err2)
        else:
            print("Successfully fixed feishu_push.py line endings remotely!")
            
        ssh.close()
    except Exception as e:
        print("Failed to run remote fix:", e)

if __name__ == "__main__":
    fix_remote_files()
