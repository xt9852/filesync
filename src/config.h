/**
 *\copyright    XT Tech. Co., Ltd.
 *\file         config.h
 *\author       xt
 *\version      1.0.0
 *\date         2022.02.08
 *\brief        配置模块定义,UTF-8(No BOM)
 */
#ifndef _CONFIG_H_
#define _CONFIG_H_
#include "xt_log.h"
#include "xt_ssh2.h"
#include "xt_monitor.h"

#define SSH_SIZE    8
#define MNT_SIZE    8

/// 配置数据
typedef struct _config
{
    xt_log          log;                    ///< 日志

    int             ssh_count;              ///< 服务器数量
    xt_ssh          ssh[SSH_SIZE];          ///< 服务器

    int             mnt_count;              ///< 监控器数量
    xt_monitor      mnt[MNT_SIZE];          ///< 监控器

} config, *p_config;                        ///< 配置指针

/**
 *\brief        初始化配置
 *\param[in]    filename    配置文件名
 *\param[out]   cfg         配置数据
 *\return       0           成功
 */
int config_init(const char *filename, p_config cfg);

#endif
