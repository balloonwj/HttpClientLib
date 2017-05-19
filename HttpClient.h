/**
 * WinInet封装类，可用于windows上http请求, HttpClient.h
 * zhangyl 2016.07.15 mail: balloonwj@qq.com
 */

#ifndef __HTTP_CLIENT_H__
#define __HTTP_CLIENT_H__

#include <WinInet.h>
#include <string>
#include <vector>

#pragma comment(lib, "wininet.lib")


#if defined(UNICODE) || defined(_UNICODE)
typedef std::wstring tstring;
#else
typedef std::string tstring;
#endif

// IE8 on Windows 7
#define     IE8_USER_AGENT  _T("Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1;Trident/4.0)")

// HTTP请求方法
enum HTTP_REQ_METHOD    
{
    REQ_METHOD_GET,
    REQ_METHOD_POST,
};

// HTTP状态事件标识
enum HTTP_STATUS_EVENT  
{
    CONNECTED_EVENT,
    REQUEST_OPENED_EVENT,
    REQUEST_COMPLETE_EVENT,
    USER_CANCEL_EVENT
};

// WinInet Http异步封装类
class CHttpClient       
{
public:
    CHttpClient(void);
    ~CHttpClient(void);

public:
    BOOL OpenRequest(LPCTSTR lpszUrl, HTTP_REQ_METHOD nReqMethod = REQ_METHOD_GET); // 打开HTTP请求函数
    BOOL AddReqHeaders(LPCTSTR lpHeaders);                                          // 添加一个或多个HTTP请求头函数
    BOOL SendRequest(const CHAR* lpData, DWORD dwLen);                              // 发送HTTP请求函数
    BOOL SendRequestEx(DWORD dwLen);                                                // 开始发送HTTP请求函数
    BOOL SendReqBodyData(const CHAR* lpBuf, DWORD dwLen, DWORD& dwSendLen);         // 发送HTTP请求消息体数据函数
    BOOL EndSendRequest();                                                          // 结束发送HTTP请求函数
    DWORD GetRespCode();                                                            // 获取HTTP响应码函数
    tstring GetRespHeader();                                                        // 获取全部HTTP头
    tstring GetRespHeader(LPCTSTR lpszName, int nIndex = 0);                        // 获取HTTP头指定字段名称的值
    int GetRespHeaderByInt(LPCTSTR lpszName, int nIndex = 0);                       // 获取HTTP头指定字段名称的值(Int)
    BOOL GetRespBodyData(CHAR * lpBuf, DWORD dwLen, DWORD& dwRecvLen);              // 获取HTTP响应消息体数据函数
    void CloseRequest();                                                            // 关闭HTTP请求函数
    void SetConnectTimeOut(DWORD dwTimeOut);                                        // 设置连接超时(单位：毫秒)
    void SetCancalEvent();                                                          // 设置取消事件函数
    void SetProxy(LPCTSTR lpszServer, WORD nPort, LPCTSTR lpszUser = NULL, LPCTSTR lpszPwd = NULL);// 设置HTTP代理服务器

private:
    // 状态回调函数
    static void __stdcall StatusCallback(HINTERNET hInternet, DWORD dwContext,  DWORD dwInternetStatus, LPVOIDlpStatusInfo, DWORD dwStatusInfoLen);
    // 解析Url函数(协议，主机名，端口，文件路径)
    BOOL ParseURL(LPCTSTR lpszUrl, LPTSTR lpszScheme, DWORD dwSchemeLength, LPTSTR lpszHostName, DWORD dwHostNameLength, WORD& nPort, LPTSTR lpszUrlPath, DWORD dwUrlPathLength);     
    // 等待事件函数
    BOOL WaitForEvent(HTTP_STATUS_EVENT nEvent, DWORD dwTimeOut);   
    // 获取HTTP响应头长度
    DWORD __GetRespHeaderLen(); 
    // 获取HTTP响应头，按行保存在m_arrRespHeader数组
    BOOL __GetRespHeader();                             

private:
    HINTERNET m_hInternet;                  // 会话句柄
    HINTERNET m_hConnect;                   // 连接句柄
    HINTERNET m_hRequest;                   // 请求句柄

    HANDLE    m_hCompleteEvent;             // 操作完成事件句柄
    HANDLE    m_hCancelEvent;               // 外部取消事件句柄

    DWORD     m_dwConnectTimeOut;           // 连接超时时间
    DWORD     m_dwContext;                  // 当前操作上下文

    std::vector<tstring> m_arrRespHeader;   // Http响应头数组

    tstring m_strProxy;                     // 代理服务器(主机名:端口)
    tstring m_strUser, m_strPwd;            // 代理服务器的用户名和密码
};

#endif //!__HTTP_CLIENT_H__
