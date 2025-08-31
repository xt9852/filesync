/**
 *\file     config.c
 *\note     UTF-8
 *\author   xt
 *\version  1.0.0
 *\date     2022.02.08
 *\brief    配置模块实现
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "config.h"
#include "cJSON.h"
#include "xt_log.h"

#ifndef false
#define false   0
#endif

/**
 *\brief                    得到文件大小
 *\param[in]    filename    文件名
 *\return                   文件大小, 小于0时失败
 */
int config_get_size(const char *filename)
{
    struct _stat buf;

    return (_stat(filename, &buf) == 0) ? buf.st_size : -1;
}

/**
 *\brief                    加载配置文件数据
 *\param[in]    filename    文件名
 *\param[in]    buf         数据指针
 *\param[in]    len         数据长度
 *\return       0           成功
 */
int config_get_data(const char *filename, char *buf, int len)
{
    if (NULL == filename || NULL == buf)
    {
        P("filename, buf is null");
        return -1;
    }

    FILE *fp = NULL;

    if (0 != fopen_s(&fp, filename, "rb"))
    {
        P("open config file fail");
        return -2;
    }

    if (len != fread(buf, 1, len, fp))
    {
        P("read config file fail");
        fclose(fp);
        return -3;
    }

    fclose(fp);
    return 0;
}

/**
 *\brief                    得到JSON数据指针
 *\param[in]    filename    文件名
 *\param[out]   root        JSON数据指针
 *\return       0           成功
 */
int config_get_json(const char *filename, cJSON **root)
{
    if (NULL == filename)
    {
        return -1;
    }

    int size = config_get_size(filename);

    if (size <= 0)
    {
        P("get config file size fail");
        return -2;
    }

    char *buf = (char*)malloc(size + 16);

    if (NULL == buf)
    {
        P("malloc buf fail");
        return -3;
    }

    int ret = config_get_data(filename, buf, size);

    *root = (0 == ret) ? cJSON_Parse(buf) : NULL;

    free(buf);

    if (NULL == *root)
    {
        P("parse json string fail");
        return -5;
    }

    return 0;
}

/**
 *\brief                    解析log节点数据
 *\param[in]    root        JSON根节点
 *\param[out]   log         配置数据
 *\return       0           成功
 */
int config_log(cJSON *root, p_xt_log log)
{
    if (NULL == root || NULL == log)
    {
        return -1;
    }

    cJSON *item = cJSON_GetObjectItem(root, "log");

    if (NULL == item)
    {
        P("config json no log node");
        return -2;
    }

    cJSON *name = cJSON_GetObjectItem(item, "name");

    if (NULL == name)
    {
        P("config json no log.name node");
        return -3;
    }

    strncpy_s(log->filename, sizeof(log->filename), name->valuestring, sizeof(log->filename) - 1);

    cJSON *level = cJSON_GetObjectItem(item, "level");

    if (NULL == level)
    {
        P("config json no log.level node");
        return -4;
    }

    if (0 == strcmp(level->valuestring, "debug"))
    {
        log->level = LOG_LEVEL_DEBUG;
    }
    else if (0 == strcmp(level->valuestring, "info"))
    {
        log->level = LOG_LEVEL_INFO;
    }
    else if (0 == strcmp(level->valuestring, "warn"))
    {
        log->level = LOG_LEVEL_WARN;
    }
    else if (0 == strcmp(level->valuestring, "error"))
    {
        log->level = LOG_LEVEL_ERROR;
    }
    else
    {
        P("config json no log.level value error");
        return -5;
    }

    cJSON *cycle = cJSON_GetObjectItem(item, "cycle");

    if (NULL == cycle)
    {
        P("config json no log.cycle node");
        return -6;
    }

    if (0 == strcmp(cycle->valuestring, "minute"))
    {
        log->cycle = LOG_CYCLE_MINUTE;
    }
    else if (0 == strcmp(cycle->valuestring, "hour"))
    {
        log->cycle = LOG_CYCLE_HOUR;
    }
    else if (0 == strcmp(cycle->valuestring, "day"))
    {
        log->cycle = LOG_CYCLE_DAY;
    }
    else if (0 == strcmp(cycle->valuestring, "week"))
    {
        log->cycle = LOG_CYCLE_WEEK;
    }
    else
    {
        P("config no log.cycle value error");
        return -7;
    }

    cJSON *backup = cJSON_GetObjectItem(item, "backup");

    if (NULL == backup)
    {
        P("config no log.backup value error");
        return -8;
    }

    log->backup = backup->valueint;

    return 0;
}

/**
 *\brief                    解析ssh节点数据
 *\param[in]    root        JSON根节点
 *\param[out]   ssh         配置数据
 *\return                   ssh数量
 */
int config_ssh(cJSON *root, p_xt_ssh ssh)
{
    if (NULL == root || NULL == ssh)
    {
        return -1;
    }

    cJSON *item = cJSON_GetObjectItem(root, "ssh");

    if (NULL == item)
    {
        P("config json no ssh node");
        return -2;
    }

    int ssh_count = cJSON_GetArraySize(item);

    if (ssh_count > SSH_SIZE)
    {
        P("item.item count too much");
        return -3;
    }

    cJSON *ssh_item;
    cJSON *addr;
    cJSON *port;
    cJSON *user;
    cJSON *pass;
    cJSON *cmd;
    cJSON *cmd_item;
    cJSON *cmd_item_cmd;
    cJSON *cmd_item_sleep;

    for (int i = 0; i < ssh_count; i++)
    {
        ssh_item = cJSON_GetArrayItem(item, i);

        if (NULL == ssh_item)
        {
            P("no server node error");
            return -4;
        }

        addr = cJSON_GetObjectItem(ssh_item, "addr");
        port = cJSON_GetObjectItem(ssh_item, "port");
        user = cJSON_GetObjectItem(ssh_item, "user");
        pass = cJSON_GetObjectItem(ssh_item, "pass");
        cmd  = cJSON_GetObjectItem(ssh_item, "cmd");

        if (NULL == addr || NULL == port || NULL == user || NULL == pass || NULL == cmd)
        {
            P("no ssh.addr,port,user,pass,type node");
            return -5;
        }

        ssh[i].port = port->valueint;
        strncpy_s(ssh[i].addr,     ADDR_SIZE,     addr->valuestring, ADDR_SIZE - 1);
        strncpy_s(ssh[i].username, USERNAME_SIZE, user->valuestring, USERNAME_SIZE - 1);
        strncpy_s(ssh[i].password, PASSWORD_SIZE, pass->valuestring, PASSWORD_SIZE - 1);

        ssh[i].cmd_count = cJSON_GetArraySize(cmd);

        if (ssh[i].cmd_count > CMD_SIZE)
        {
            P("no ssh.cmd count too much");
            return -6;
        }

        for (int j = 0; j < ssh[i].cmd_count; j++)
        {
            cmd_item = cJSON_GetArrayItem(cmd, j);

            if (NULL == cmd_item)
            {
                P("no ssh.cmd node");
                return -7;
            }

            cmd_item_cmd = cJSON_GetObjectItem(cmd_item, "cmd");
            cmd_item_sleep = cJSON_GetObjectItem(cmd_item, "sleep");

            ssh[i].cmd[j].sleep = cmd_item_sleep->valueint;
            strncpy_s(ssh[i].cmd[j].str, CMD_STR_SIZE, cmd_item_cmd->valuestring, CMD_STR_SIZE - 1);
        }
    }

    return ssh_count;
}

/**
 *\brief                    解析monitor节点数据
 *\param[in]    root        JSON根节点
 *\param[out]   monitor     配置数据
 *\param[in]    ssh_count   ssh数量
 *\return                   monitor数量
 */
int config_monitor(cJSON *root, p_xt_monitor monitor, int ssh_count)
{
    if (NULL == root || NULL == monitor)
    {
        return -1;
    }

    cJSON *item = cJSON_GetObjectItem(root, "monitor");

    if (NULL == item)
    {
        P("config json no monitor node");
        return -2;
    }

    int monitor_count = cJSON_GetArraySize(item);

    if (monitor_count > MNT_SIZE)
    {
        P("monitor.item count too much");
        return -3;
    }

    int len;
    int whitelist_count;
    int blacklist_count;
    cJSON *monitor_item;
    cJSON *ssh;
    cJSON *localpath;
    cJSON *remotpath;
    cJSON *whitelist;
    cJSON *blacklist;
    cJSON *white_item;
    cJSON *black_item;

    for (int i = 0; i < monitor_count; i++)
    {
        monitor_item = cJSON_GetArrayItem(item, i);

        if (NULL == monitor_item)
        {
            P("no monitor node");
            return -4;
        }

        ssh       = cJSON_GetObjectItem(monitor_item, "ssh");
        localpath = cJSON_GetObjectItem(monitor_item, "localpath");
        remotpath = cJSON_GetObjectItem(monitor_item, "remotepath");
        whitelist = cJSON_GetObjectItem(monitor_item, "whitelist");
        blacklist = cJSON_GetObjectItem(monitor_item, "blacklist");

        if (NULL == ssh || NULL == localpath || NULL == remotpath || NULL == whitelist || NULL == blacklist)
        {
            P("no monitor.ssh,localpath,remotepath,whitelist,blacklist node");
            return -5;
        }

        strncpy_s(monitor[i].localpath,  LOCALPATH_SIZE,  localpath->valuestring, LOCALPATH_SIZE - 1);
        strncpy_s(monitor[i].remotepath, REMOTEPATH_SIZE, remotpath->valuestring, REMOTEPATH_SIZE - 1);

        len = (int)strlen(localpath->valuestring);

        if (monitor[i].localpath[len - 1] != '\\') // 当不是\结尾时添加
        {
            monitor[i].localpath[len] = '\\';
            monitor[i].localpath[len + 1] = '\0';
        }

        len = (int)strlen(remotpath->valuestring);

        if (monitor[i].remotepath[len - 1] != '/') // 当不是/结尾时添加
        {
            monitor[i].remotepath[len] = '/';
            monitor[i].remotepath[len + 1] = '\0';
        }

        if (ssh->valueint < 0 || ssh->valueint >= ssh_count)
        {
            P("monitor.ssh error");
            return -6;
        }

        monitor[i].ssh_id = ssh->valueint;

        whitelist_count = cJSON_GetArraySize(whitelist);
        blacklist_count = cJSON_GetArraySize(blacklist);

        if (whitelist_count > WHITELIST_SIZE)
        {
            P("no monitor.whitelist count too much");
            return -7;
        }

        if (blacklist_count > BLACKLIST_SIZE)
        {
            P("no monitor.blacklist count too much");
            return -8;
        }

        for (int j = 0; j < whitelist_count; j++)
        {
            white_item = cJSON_GetArrayItem(whitelist, j);
            strncpy_s(monitor[i].whitelist[j], WHITELIST_STR_SIZE, white_item->valuestring, WHITELIST_STR_SIZE - 1);
        }

        for (int j = 0; j < blacklist_count; j++)
        {
            black_item = cJSON_GetArrayItem(blacklist, j);
            strncpy_s(monitor[i].blacklist[j], BLACKLIST_STR_SIZE, black_item->valuestring, BLACKLIST_STR_SIZE - 1);
        }

        monitor[i].whitelist_count = whitelist_count;
        monitor[i].blacklist_count = blacklist_count;
    }

    return monitor_count;
}

/**
 *\brief                    初始化配置
 *\param[in]    filename    配置文件名
 *\param[out]   cfg         配置数据
 *\return       0           成功
 */
int config_init(const char *filename, p_config cfg)
{
    if (NULL == filename || NULL == cfg)
    {
        P("param null");
        return -1;
    }

    cJSON *root;

    int ret = config_get_json(filename, &root);

    if (0 != ret)
    {
        return -2;
    }

    ret = config_log(root, cfg->log);

    if (0 != ret)
    {
        return -3;
    }

    ret = config_ssh(root, cfg->ssh);

    if (ret <= 0)
    {
        return -3;
    }

    cfg->ssh_count = ret;

    ret = config_monitor(root, cfg->mnt, cfg->ssh_count);

    if (ret <= 0)
    {
        return -3;
    }

    cfg->mnt_count = ret;

    return 0;
}
