#include <thread>  // 必须加上这一行，否则不认识 yield()
#pragma once

/**
 * @brief AI 推理与去抖状态机线程函数
 * 负责：加载NCNN模型、绑核CPU2&3、从缓存取图、执行YOLO推理、逻辑去抖、生成报警
 */
void inference_thread_func();