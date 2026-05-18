/**
 * @file      io_node.hpp
 * @brief     PPE智能安全监控中枢系统
 * @details   飞腾派 E2000Q 异构多核平台定制化开发
 * @author    [双生序章] 团队
 * @version   3.1.0 (极致稳定版)
 * @date      2026-05-18
 * @copyright Copyright (c) 2026. All rights reserved.
 */

#pragma once

/**
 * @brief 后台 I/O 与日志记录线程函数
 * 负责：从报警队列取数据、创建文件夹、执行 JPEG 压缩、写入 CSV
 */
void io_thread_func(); // 统一改为这个短名字