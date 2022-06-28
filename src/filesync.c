/**
 *\copyright    XT Tech. Co., Ltd.
 *\file         filesync.c
 *\author       xt
 *\version      1.0.0
 *\date         2022.02.08
 *\brief        主模块,UTF-8(No BOM)
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "xt_log.h"
#include "xt_list.h"
#include "xt_memory_pool.h"
#include "xt_monitor.h"
#include "xt_ssh2.h"
#include "config.h"


config              g_cfg                   = {0};  ///< 配置信息

xt_list             g_monitor_event_list    = {0};  ///< 监控事件队列

xt_memory_pool      g_memory_pool           = {0};  ///< 内存池

int output(void *param, const char *data, unsigned int len)
{
    D(data);
    printf("%s\n", data);
    return 0;
}

void process_path(char *path, int len)
{
    for (int i = 0; i < len; i++)
    {
        if (path[i] == '\\')
        {
            path[i] = '/';
        }
    }
}

// monitor->queue->process->sshclient
int main(int argc, char *argv[])
{
    int ret = config_init("filesync.json", &g_cfg);

    if (0 != ret)
    {
        printf("%s|init config fail\n", __FUNCTION__);
        return -1;
    }

    g_cfg.log.root = 21;    // 根目录长度

    ret = log_init(&(g_cfg.log));

    if (0 != ret)
    {
        printf("%s|init log fail\n", __FUNCTION__);
        return -2;
    }

    D("init log ok");

    ret = list_init(&g_monitor_event_list);

    if (0 != ret)
    {
        printf("%s|init log fail\n", __FUNCTION__);
        return -3;
    }

    D("init list ok");

    ret = memory_pool_init(&g_memory_pool, 1024, 100);

    if (0 != ret)
    {
        printf("%s|init memory pool fail\n", __FUNCTION__);
        return -4;
    }

    D("init memory pool ok");

    for (int i = 0; i < g_cfg.ssh_count; i++)   // 可连接多个服务器
    {
        ret = ssh_init(output, NULL, &(g_cfg.ssh[i]));

        if (0 != ret)
        {
            return -5;
        }
    }

    D("init ssh ok");

    for (int i = 0; i < g_cfg.mnt_count; i++)   // 可监控多个目录
    {
        ret = monitor_init(&g_cfg.mnt[i], &g_monitor_event_list, &g_memory_pool);

        if (0 != ret)
        {
            return -6;
        }
    }

    D("init monitor ok");

    int                len;
    char               cmd[1024];
    char               buf[10240];
    char*              obj_name_old;
    char               obj_name_local[MNT_OBJNAME_SIZE];
    char               obj_name_remote[MNT_OBJNAME_SIZE];

    p_xt_ssh           ssh;
    p_xt_monitor       mnt;
    p_xt_monitor_event event;

    while (true)
    {
        if (0 != list_head_pop(&g_monitor_event_list, &event))
        {
            sleep(1);
            continue;
        }

        mnt = &(g_cfg.mnt[event->monitor_id]);
        ssh = &(g_cfg.ssh[mnt->ssh_id]);

        D("type:%d name:%s cmd:%d monitor_id:%d ssh_id:%d", event->obj_type, event->obj_name, event->cmd, event->monitor_id, mnt->ssh_id);

        switch (event->cmd)
        {
            case EVENT_CMD_CREATE:  // 新建文件或目录
            {
                len = SP(cmd, "%s %s%s", ((event->obj_type == EVENT_OBJECT_DIR) ? "mkdir" : ">"), mnt->remotepath, event->obj_name);
                process_path(cmd, len);
                len = sizeof(buf);
                ssh_send_cmd(ssh, cmd, strlen(cmd), buf, &len);
                break;
            }
            case EVENT_CMD_DELETE: // 删除文件或目录
            {
                len = SP(cmd, "rm -rf %s%s", mnt->remotepath, event->obj_name);
                process_path(cmd, len);
                len = sizeof(buf);
                ssh_send_cmd(ssh, cmd, strlen(cmd), buf, &len);
                break;
            }
            case EVENT_CMD_RENAME: // 重命名文件或目录
            {
                obj_name_old = strchr(event->obj_name, '|');
                *obj_name_old++ = '\0'; // 指向旧文件名
                len = SP(cmd, "mv -f %s%s %s%s", mnt->remotepath, obj_name_old, mnt->remotepath, event->obj_name);
                process_path(cmd, len);
                len = sizeof(buf);
                ssh_send_cmd(ssh, cmd, strlen(cmd), buf, &len);
                break;
            }
            case EVENT_CMD_MODIFY: // 删除文件或目录
            {
                SP(obj_name_local,  "%s%s", mnt->localpath,  event->obj_name);
                len = SP(obj_name_remote, "%s%s", mnt->remotepath, event->obj_name);
                process_path(obj_name_remote, len);
                len = sizeof(buf);
                ssh_send_cmd_rz(ssh, obj_name_local, obj_name_remote);
                break;
            }
        }
    }

    return 0;
}
