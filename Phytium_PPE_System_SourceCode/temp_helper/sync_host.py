import paramiko
import base64
import os
import sys

sys.stdout.reconfigure(encoding='utf-8')
sys.stderr.reconfigure(encoding='utf-8')

def upload_via_echo(ssh, local_path, remote_path):
    if not os.path.exists(local_path):
        print(f"Local file {local_path} does not exist!")
        return False
        
    with open(local_path, "rb") as f:
        content = f.read()
    b64_data = base64.b64encode(content).decode('utf-8')
    
    temp_b64_path = f"/tmp/{os.path.basename(local_path)}.b64"
    ssh.exec_command(f"echo -n '' > {temp_b64_path}")
    
    print(f"Appending {local_path} via echo chunks...")
    chunk_size = 800
    chunks = [b64_data[i:i+chunk_size] for i in range(0, len(b64_data), chunk_size)]
    
    for idx, chunk in enumerate(chunks):
        cmd = f"echo -n '{chunk}' >> {temp_b64_path}"
        stdin, stdout, stderr = ssh.exec_command(cmd)
        exit_status = stdout.channel.recv_exit_status()
        if exit_status != 0:
            print(f"Failed to append chunk {idx+1}/{len(chunks)}: {stderr.read().decode()}")
            return False
            
    remote_dir = os.path.dirname(remote_path)
    ssh.exec_command(f"mkdir -p {remote_dir}")
    
    print(f"Decoding Base64 on remote: {remote_path}")
    stdin, stdout, stderr = ssh.exec_command(f"base64 -d {temp_b64_path} > {remote_path} && rm -f {temp_b64_path}")
    exit_status = stdout.channel.recv_exit_status()
    
    if exit_status == 0:
        print(f"Uploaded {local_path} successfully!")
        return True
    else:
        print(f"Failed to decode base64 on remote board for {local_path}:", stderr.read().decode())
        return False

def main():
    host = "172.20.10.2"
    port = 22
    username = "user"
    password = "user"
    
    local_base = r"d:\飞腾派\CICC1004607+初赛+技术数据(代码类)\ppe4-28\Phytium_PPE_System_SourceCode"
    files = [
        ("ppe_system/src/rpmsg_node.cpp", "/home/user/Phytium_PPE_System_SourceCode/ppe_system/src/rpmsg_node.cpp"),
        ("ppe_system/include/ui_main_window.hpp", "/home/user/Phytium_PPE_System_SourceCode/ppe_system/include/ui_main_window.hpp"),
        ("ppe_system/src/ui_main_window.cpp", "/home/user/Phytium_PPE_System_SourceCode/ppe_system/src/ui_main_window.cpp"),
        ("ppe_system/scripts/feishu_push.py", "/home/user/Phytium_PPE_System_SourceCode/ppe_system/scripts/feishu_push.py"),
        ("ppe_system/run_real_deepseek.sh", "/home/user/Phytium_PPE_System_SourceCode/ppe_system/run_real_deepseek.sh"),
        ("temp_helper/test_deepseek.py", "/home/user/Phytium_PPE_System_SourceCode/temp_helper/test_deepseek.py"),
        ("ppe_system/src/deepseek_worker.cpp", "/home/user/Phytium_PPE_System_SourceCode/ppe_system/src/deepseek_worker.cpp"),
        ("ppe_system/src/main.cpp", "/home/user/Phytium_PPE_System_SourceCode/ppe_system/src/main.cpp")
    ]
    
    print(f"Connecting to SSH {host}:{port}...")
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    try:
        ssh.connect(host, port=port, username=username, password=password, timeout=10)
        print("SSH connection successful!")
        
        for rel_path, remote_path in files:
            local_path = os.path.join(local_base, rel_path)
            if not upload_via_echo(ssh, local_path, remote_path):
                print(f"Sync failed at file {rel_path}!")
                ssh.close()
                return False
                
        # Make script executable
        print("Setting executable permission on run_real_deepseek.sh...")
        ssh.exec_command("chmod +x /home/user/Phytium_PPE_System_SourceCode/ppe_system/run_real_deepseek.sh")

        # Now compile the host ppe_system
        print("\n--- Compiling Host Application (ppe_system) ---")
        build_cmd = "cd /home/user/Phytium_PPE_System_SourceCode/ppe_system/build && make -j4"
        stdin, stdout, stderr = ssh.exec_command(build_cmd)
        
        for line in iter(stdout.readline, ""):
            print(line, end="")
            
        err = stderr.read().decode()
        if err:
            print("Errors/Warnings during build:\n", err)
            
        exit_status = stdout.channel.recv_exit_status()
        print(f"Build exit status: {exit_status}")
        
        ssh.close()
        return exit_status == 0
    except Exception as e:
        print(f"FAILED to connect or sync: {e}")
        return False

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
