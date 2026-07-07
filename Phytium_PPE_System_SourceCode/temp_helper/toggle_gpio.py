import subprocess
import time
import sys

# 清理任何残留的 gpioset 占用进程
subprocess.run("echo 'user' | sudo -S pkill gpioset", shell=True, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)

print("Starting GPIO toggle loop on gpiochip4 13...", flush=True)
try:
    while True:
        print("Setting High (1)...", flush=True)
        # 用 sudo gpioset -m signal 后台运行，以便在这个 2 秒内保持 high 电平驱动
        p1 = subprocess.Popen("echo 'user' | sudo -S gpioset -m signal gpiochip4 13=1", shell=True)
        time.sleep(2)
        
        # 释放当前的占用
        subprocess.run("echo 'user' | sudo -S pkill gpioset", shell=True, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)
        p1.wait()
        
        print("Setting Low (0)...", flush=True)
        # 切换到低电平，保持 2 秒
        p2 = subprocess.Popen("echo 'user' | sudo -S gpioset -m signal gpiochip4 13=0", shell=True)
        time.sleep(2)
        
        # 释放当前的占用
        subprocess.run("echo 'user' | sudo -S pkill gpioset", shell=True, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)
        p2.wait()
except KeyboardInterrupt:
    print("Loop stopped by user.", flush=True)
finally:
    # 退出时彻底释放引脚占用
    subprocess.run("echo 'user' | sudo -S pkill gpioset", shell=True, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL)
