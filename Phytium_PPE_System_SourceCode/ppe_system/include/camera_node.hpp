#pragma once

/**
 * @brief 摄像头采集线程函数
 * 负责：初始化硬件、绑核CPU0、设置实时优先级、将图像推入 cap_queue
 */
void camera_thread_func();