import paramiko
import os
import sys
import time

def main():
    host = "172.20.10.2"
    port = 22
    username = "user"
    password = "user"
    
    local_base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    files = [
        ("ppe_system/include/ui_main_window.hpp", "/home/user/Phytium_PPE_System_SourceCode/ppe_system/include/ui_main_window.hpp"),
        ("ppe_system/src/ui_main_window.cpp", "/home/user/Phytium_PPE_System_SourceCode/ppe_system/src/ui_main_window.cpp"),
        ("ppe_system/src/deepseek_worker.cpp", "/home/user/Phytium_PPE_System_SourceCode/ppe_system/src/deepseek_worker.cpp"),
        ("ppe_system/include/global_context.hpp", "/home/user/Phytium_PPE_System_SourceCode/ppe_system/include/global_context.hpp"),
        ("ppe_system/src/global_context.cpp", "/home/user/Phytium_PPE_System_SourceCode/ppe_system/src/global_context.cpp"),
        ("ppe_system/src/camera_node.cpp", "/home/user/Phytium_PPE_System_SourceCode/ppe_system/src/camera_node.cpp"),
        ("ppe_system/src/rpmsg_node.cpp", "/home/user/Phytium_PPE_System_SourceCode/ppe_system/src/rpmsg_node.cpp"),
        ("ppe_system/include/rpmsg_node.hpp", "/home/user/Phytium_PPE_System_SourceCode/ppe_system/include/rpmsg_node.hpp"),
        ("ppe_system/src/inference_node.cpp", "/home/user/Phytium_PPE_System_SourceCode/ppe_system/src/inference_node.cpp"),
        ("ppe_system/scripts/pull_model.sh", "/home/user/Phytium_PPE_System_SourceCode/ppe_system/scripts/pull_model.sh")
    ]
    
    print(f"Connecting to SSH {host}:{port}...")
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    try:
        ssh.connect(host, port=port, username=username, password=password, timeout=15)
        print("Connected via SSH. Opening SFTP...")
        sftp = ssh.open_sftp()
        
        # 1. Upload files
        for rel_path, remote_path in files:
            local_path = os.path.join(local_base, rel_path)
            print(f"Uploading {local_path} -> {remote_path}...")
            sftp.put(local_path, remote_path)
            
        sftp.close()
        print("SFTP Upload completed.")
        
        # 2. Remote Compilation
        print("\n--- Compiling Host Application (ppe_system) ---")
        build_cmd = "cd /home/user/Phytium_PPE_System_SourceCode/ppe_system/build && make -j4"
        stdin, stdout, stderr = ssh.exec_command(build_cmd)
        
        for line in iter(stdout.readline, ""):
            print(line, end="")
            
        err = stderr.read().decode('utf-8', errors='ignore')
        if err:
            print("Errors/Warnings during build:\n", err)
            
        exit_status = stdout.channel.recv_exit_status()
        print(f"Build exit status: {exit_status}")
        if exit_status != 0:
            print("Compilation failed! Aborting restart.")
            ssh.close()
            return
            
        # 3. Kill old ppe_system and restart
        print("\n--- Restarting Host GUI Application and Slave Core ---")
        ssh.exec_command("echo user | sudo -S killall ppe_system")
        time.sleep(2)
        
        # Reboot slave core to refresh RPMsg channel
        print("Rebooting slave core (remoteproc0)...")
        ssh.exec_command("echo user | sudo -S sh -c 'echo stop > /sys/class/remoteproc/remoteproc0/state'")
        time.sleep(1)
        ssh.exec_command("echo user | sudo -S sh -c 'echo start > /sys/class/remoteproc/remoteproc0/state'")
        time.sleep(2)
        
        launch_cmd = 'nohup sh -c "export DISPLAY=:0 && xhost + 2>/dev/null && cd /home/user/Phytium_PPE_System_SourceCode/ppe_system/build && ./ppe_system" > /tmp/ppe_system.log 2>&1 &'
        print("Launching ppe_system...")
        ssh.exec_command(launch_cmd)
        time.sleep(3)
        
        # Verify if running
        stdin, stdout, stderr = ssh.exec_command("pgrep -l ppe_system")
        pgrep_out = stdout.read().decode().strip()
        if pgrep_out:
            print(f"Success! ppe_system is running: {pgrep_out}")
        else:
            print("Warning: ppe_system is NOT running! Log contents:")
            stdin, stdout, stderr = ssh.exec_command("cat /tmp/ppe_system.log")
            print(stdout.read().decode())
            
        ssh.close()
    except Exception as e:
        print(f"An error occurred: {e}", file=sys.stderr)

if __name__ == "__main__":
    main()
