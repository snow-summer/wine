/*
 * Copyright 2018 Jacek Caban for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS

#include "initguid.h"
#include "taskschd.h"

#include "wine/test.h"

static ITaskService *service;
static ITaskFolder *root;

static WCHAR *a2w(const char *str)
{
    WCHAR *ret;
    int len;

    if(!str)
        return NULL;

    len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
    ret = HeapAlloc(GetProcessHeap(), 0, len*sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, str, -1, ret, len);

    return ret;
}

#define run_command(a) _run_command(__LINE__,a)
static DWORD _run_command(unsigned line, const char *cmd)
{
    STARTUPINFOA si = {sizeof(STARTUPINFOA)};
    PROCESS_INFORMATION pi;
    char command[1024];
    BOOL r;
    DWORD ret;

    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = INVALID_HANDLE_VALUE;
    si.hStdOutput = INVALID_HANDLE_VALUE;
    si.hStdError  = INVALID_HANDLE_VALUE;

    strcpy(command, cmd);
    r = CreateProcessA(NULL, command, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    ok_(__FILE__,line)(r, "CreateProcess failed: %u\n", GetLastError());
    if(!r) return -1;

    ret = WaitForSingleObject(pi.hProcess, 10000);
    ok_(__FILE__,line)(ret == WAIT_OBJECT_0, "wait failed\n");
    if (ret == WAIT_TIMEOUT)
        TerminateProcess(pi.hProcess, -1);

    r = GetExitCodeProcess(pi.hProcess, &ret);
    ok_(__FILE__,line)(r, "GetExitCodeProcess failed: %u\n", GetLastError());

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return ret;
}

#define register_task(a) _register_task(__LINE__,a)
static void _register_task(unsigned line, const char *task_name_a)
{
    IRegisteredTask *task;
    VARIANT empty;
    WCHAR *task_name, *xml;
    HRESULT hres;

    static const char xml_a[] =
        "<?xml version=\"1.0\"?>\n"
        "<Task xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\n"
        "  <RegistrationInfo>\n"
        "    <Description>\"Task1\"</Description>\n"
        "  </RegistrationInfo>\n"
        "  <Settings>\n"
        "    <Enabled>false</Enabled>\n"
        "    <Hidden>false</Hidden>\n"
        "  </Settings>\n"
        "  <Actions>\n"
        "    <Exec>\n"
        "      <Command>\"task1.exe\"</Command>\n"
        "    </Exec>\n"
        "  </Actions>\n"
        "</Task>\n";

    V_VT(&empty) = VT_EMPTY;
    task_name = a2w(task_name_a);
    xml = a2w(xml_a);

    /* make sure it's not registered */
    ITaskFolder_DeleteTask(root, task_name, 0);

    hres = ITaskFolder_RegisterTask(root, task_name, xml, TASK_CREATE, empty, empty,
                                    TASK_LOGON_NONE, empty, &task);
    ok_(__FILE__,line)(hres == S_OK, "RegisterTask failed: %08x\n", hres);
    HeapFree(GetProcessHeap(), 0, task_name);
    HeapFree(GetProcessHeap(), 0, xml);

    IRegisteredTask_Release(task);
}

#define unregister_task(a) _unregister_task(__LINE__,a)
static void _unregister_task(unsigned line, const char *task_name_a)
{
    WCHAR *task_name;
    HRESULT hres;

    task_name = a2w(task_name_a);

    hres = ITaskFolder_DeleteTask(root, task_name, 0);
    ok_(__FILE__,line)(hres == S_OK, "DeleteTask failed: %08x\n", hres);

    HeapFree(GetProcessHeap(), 0, task_name);
}

static BOOL initialize_task_service(void)
{
    VARIANT empty;
    HRESULT hres;

    hres = CoCreateInstance(&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
                            &IID_ITaskService, (void **)&service);
    if(hres != S_OK) {
        ok(hres == REGDB_E_CLASSNOTREG, "Could not create task service: %08x\n", hres);
        win_skip("Task service not available\n");
        return FALSE;
    }

    V_VT(&empty) = VT_EMPTY;
    hres = ITaskService_Connect(service, empty, empty, empty, empty);
    ok(hres == S_OK, "Connect failed: %08x\n", hres);

    hres = ITaskService_GetFolder(service, NULL, &root);
    ok(hres == S_OK, "GetFolder error %08x\n", hres);
    return TRUE;
}

START_TEST(schtasks)
{
    static WCHAR wineW[] = {'\\','w','i','n','e',0};
    static WCHAR wine_testW[] = {'\\','w','i','n','e','\\','t','e','s','t',0};
    DWORD r;

    CoInitialize(NULL);
    if(!initialize_task_service()) {
        CoUninitialize();
        return;
    }

    r = run_command("schtasks");
    ok(r == 0, "r = %u\n", r);

    register_task("winetest");

    r = run_command("schtasks /change /tn winetest /enable");
    ok(r == 0, "r = %u\n", r);

    unregister_task("winetest");

    r = run_command("schtasks /change /tn winetest /enable");
    ok(r == 1, "r = %u\n", r);

    register_task("wine\\test\\winetest");

    r = run_command("schtasks /CHANGE /tn wine\\test\\winetest /enable");
    ok(r == 0, "r = %u\n", r);

    r = run_command("schtasks /delete /f /tn wine\\test\\winetest");
    ok(r == 0, "r = %u\n", r);

    r = run_command("schtasks /Change /tn wine\\test\\winetest /enable");
    ok(r == 1, "r = %u\n", r);

    ITaskFolder_DeleteFolder(root, wine_testW, 0);
    ITaskFolder_DeleteFolder(root, wineW, 0);
    ITaskFolder_Release(root);
    ITaskService_Release(service);
    CoUninitialize();
}
