#pragma once

/**
 * @brief 后台 I/O 与日志记录线程函数
 * 负责：从报警队列取数据、创建文件夹、执行 JPEG 压缩、写入 CSV
 */
void io_thread_func(); // 统一改为这个短名字