/**
 *\file     filesync.c
 *\note     UTF-8
 *\author   xt
 *\version  1.0.0
 *\brief    主模块
 *          时间|事件
 *          -|-
 *          2022.02.08|创建文件
 *          2024.06.23|添加Doxygen注释
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "xt_log.h"
#include "xt_list.h"
#include "xt_memory_pool.h"
#include "xt_monitor.h"
#include "xt_ssh2.h"
#include "xt_notify.h"
#include "xt_utitly.h"
#include "config.h"
#include "resource.h"

char                g_path[MAX_PATH]        = "";       ///< 文件路径

char               *g_title                 = NULL;     ///< 标题

xt_log              g_log                   = {0};      ///< 日志文件

xt_list             g_monitor_event_list    = {0};      ///< 监控事件队列

xt_memory_pool      g_memory_pool           = {0};      ///< 内存池

config              g_cfg                   = {&g_log}; ///< 配置信息

/**
 *\brief                        SSH输出回调
 *\param[in]    param           用户自定义参数
 *\param[in]    data            数据
 *\param[in]    len             数据长度
 *\return       0               成功
 */
int output(void *param, const char *data, unsigned int len)
{
    return 0;
}

/**
 *\brief                        将windows路径转成linux路径
 *\param[in]    path            路径
 *\param[in]    len             路径长度
 *\return                       无
 */
void path_to_linux(char *path, int len)
{
    for (int i = 0; i < len; i++)
    {
        if (path[i] == '\\')
        {
            path[i] = '/';
        }
    }
}

/**
 *\brief                        窗体关闭处理函数
 *\param[in]    wnd             窗体句柄
 *\param[in]    param           自定义参数
 *\return                       无
 */
void on_menu_exit(HWND wnd, void *param)
{
    if (IDNO == MessageBoxW(wnd, L"确定退出?", L"消息", MB_ICONQUESTION | MB_YESNO))
    {
        return;
    }

    DestroyWindow(wnd);
}

/**
 *\brief                        监控事件处理线程
 *\param[in]    param           无
 *\return                       空
 */
void* process_monitor_event_thread(void *param)
{
    D("begin");

    int                len;
    char               cmd[1024];
    char               buf[10240];
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
                path_to_linux(cmd, len);
                ssh_send_cmd(ssh, cmd, strlen(cmd), buf, sizeof(buf));
                break;
            }
            case EVENT_CMD_DELETE: // 删除文件或目录
            {
                len = SP(cmd, "rm -rf %s%s", mnt->remotepath, event->obj_name);
                path_to_linux(cmd, len);
                ssh_send_cmd(ssh, cmd, strlen(cmd), buf, sizeof(buf));
                break;
            }
            case EVENT_CMD_RENAME: // 重命名文件或目录
            {
                len = SP(cmd, "mv -f %s%s %s%s", mnt->remotepath, event->obj_oldname, mnt->remotepath, event->obj_name);
                path_to_linux(cmd, len);
                ssh_send_cmd(ssh, cmd, strlen(cmd), buf, sizeof(buf));
                break;
            }
            case EVENT_CMD_MODIFY: // 删除文件或目录
            {
                SP(obj_name_local,  "%s%s", mnt->localpath,  event->obj_name);
                len = SP(obj_name_remote, "%s%s", mnt->remotepath, event->obj_name);
                path_to_linux(obj_name_remote, len);
                ssh_send_cmd_rz(ssh, obj_name_local, obj_name_remote);
                break;
            }
        }
    }

    D("exit");
    return NULL;
}

/**
 *\brief                        窗体类程序主函数
 *\param[in]    hInstance       当前实例句柄
 *\param[in]    hPrevInstance   先前实例句柄
 *\param[in]    lpCmdLine       命令行参数
 *\param[in]    nCmdShow        显示状态(最小化,最大化,隐藏)
 *\return                       exe程序返回值
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    GetModuleFileNameA(hInstance, g_path, sizeof(g_path));

    g_title = strrchr(g_path, '\\');
    *g_title++ = '\0';

    char *end = strrchr(g_title, '.');
    *end = '\0';

    char tmp[MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s\\%s.json", g_path, g_title);

    int ret = config_init(tmp, &g_cfg);

    if (ret != 0)
    {
        MessageBoxW(NULL, L"配置错误", L"错误", MB_OK);
        return -1;
    }

    ret = log_init(g_path, 21, g_cfg.log);  // 21是当前代码的根目录长度,日志中只保留代码的相对路径

    if (0 != ret)
    {
        MessageBoxW(NULL, L"日志错误", L"错误", MB_OK);
        return -2;
    }

    D("init log ok");

    ret = list_init(&g_monitor_event_list);

    if (0 != ret)
    {
        E("%s|init log fail", __FUNCTION__);
        return -3;
    }

    D("init list ok");

    ret = memory_pool_init(&g_memory_pool, 1024, 100);

    if (0 != ret)
    {
        E("%s|init memory pool fail", __FUNCTION__);
        return -4;
    }

    D("init memory pool ok");

    for (int i = 0; i < g_cfg.ssh_count; i++)   // 可连接多个服务器
    {
        ret = ssh_init(output, NULL, &(g_cfg.ssh[i]));

        if (0 != ret)
        {
            E("%s|init ssh fail", __FUNCTION__);
            return -5;
        }
    }

    D("init ssh ok");

    for (int i = 0; i < g_cfg.mnt_count; i++)   // 可监控多个目录
    {
        ret = monitor_init(&g_cfg.mnt[i], &g_monitor_event_list, &g_memory_pool);

        if (0 != ret)
        {
            E("%s|init monitor fail", __FUNCTION__);
            return -6;
        }
    }

    D("init monitor ok");

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);    // 退出时自行释放所占用的资源

    ret = pthread_create(&tid, &attr, process_monitor_event_thread, NULL);

    if (ret != 0)
    {
        E("create thread fail, error:%d", ret);
        return -7;
    }

    notify_menu_info menu[] = { {L"退出(&E)", NULL, on_menu_exit} };

    ret = notify_init(hInstance, IDI_GREEN, "filesync", SIZEOF(menu), menu);

    if (0 != ret)
    {
        E("%s|init notify fail", __FUNCTION__);
        return -8;
    }

    D("init notify ok");

    return notify_loop_msg();
}
