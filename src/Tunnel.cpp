﻿#include "stdafx.h"
#include "Tunnel.h"


CTunnel::CTunnel(void):
	m_s1(INVALID_SOCKET),
	m_s2(INVALID_SOCKET)
{
}


CTunnel::~CTunnel(void)
{
}

void CTunnel::Close()
{
	Socket::Close(m_s1);
	Socket::Close(m_s2);
}
bool CTunnel::BindTunnel( int port1, int port2 )
{
	m_port = port1;

	bool ret = FALSE;

	do 
	{
		m_s1 = Create();

		m_s2 = Create();

		if(m_s1 == INVALID_SOCKET) 
			break;

		if(m_s2 == INVALID_SOCKET)
			break;

		if(!Listen(m_s1, port1))
			break;

		if(!Listen(m_s2, port2))
			break;

		ret = TRUE;

	} while (FALSE);

	if ( !ret )
	{
		Socket::Close(m_s1);
		Socket::Close(m_s2);
	}

	return ret;
}

bool CTunnel::WaitTunnel()
{
	sockaddr_in svr;
	Thread t;

	m_sMgr = Accept(m_s1,(sockaddr*)&svr);

	if (m_sMgr != SOCKET_ERROR)
	{
		t.Start((LPTHREAD_START_ROUTINE)CheckMgr,this);
	}

	return m_sMgr != SOCKET_ERROR;
}

DWORD WINAPI CTunnel::Tunnel(LPVOID lpParameter)
{
	return CTunnel::GetInstanceRef().TunnelProc();
}

DWORD WINAPI CTunnel::TunnelProc()
{
	PROXY_CONFIG config;
	TUNNEL_CONFIG* pTransInfo = NULL;
	int s = INVALID_SOCKET;

	Thread t;

	while(TRUE)
	{
		sockaddr_in svr;

		int s = Accept(m_s1,(sockaddr*)&svr);
		
		if (s <= 0)
			break;

		bool bRet = RecvBuf(s,(char*)&config,sizeof(PROXY_CONFIG));

		if (!bRet)
			break;

		pTransInfo = new TUNNEL_CONFIG;

		pTransInfo->s1 = s;
		pTransInfo->s2 = config.s;
		pTransInfo->lpParameter = this;

		t.Start((LPTHREAD_START_ROUTINE)TCPTunnel,pTransInfo);

	}

	Socket::Close(s);
	Close();

	return 0;
}
DWORD WINAPI CTunnel::Worker(LPVOID lpParameter)
{
	return CTunnel::GetInstanceRef().WorkerProc();
}

DWORD WINAPI CTunnel::WorkerProc()
{
	sockaddr_in svr;
	PROXY_CONFIG config;
	int s = INVALID_SOCKET;
	while(TRUE)
	{
		s = Accept(m_s2,(sockaddr*)&svr);

		infoLog(_T("Accept : %s"),a2t(inet_ntoa(svr.sin_addr)));

		if (s <= 0)
			break;

		config.s = s;
		config.port = m_port;

		bool bRet = SendBuf(m_sMgr,(char*)&config,sizeof(PROXY_CONFIG));

		if(!bRet)
			break;

	}

	Socket::Close(s);
	Close();

	return 0;
}

DWORD WINAPI CTunnel::TCP_S2C(LPVOID lpParameter) 
{
	TUNNEL_CONFIG* pInfo = (TUNNEL_CONFIG*)lpParameter;
	char buffer[1024*4] = {0};
	int nCount = 0;

	while (TRUE)
	{
		nCount = recv(pInfo->s2,buffer,1024*4,0);
		if (nCount == SOCKET_ERROR)
		{
			debugLog(_T("recv Error! %d"),WSAGetLastError());
			break;
		}
		if(!Socket::SendBuf(pInfo->s1,buffer,nCount))
		{
			debugLog(_T("send Error! %d"),WSAGetLastError());
			break;
		}
	}
	Socket::Close(pInfo->s1);
	Socket::Close(pInfo->s2);
	return 0;
}

DWORD WINAPI CTunnel::TCP_C2S(void* lpParameter) 
{
	TUNNEL_CONFIG* pInfo = (TUNNEL_CONFIG*)lpParameter;
	char buffer[1024*4] = {0};
	int nCount = 0;

	while (TRUE)
	{
		nCount = recv(pInfo->s1,buffer,1024*4,0);
		if (nCount == SOCKET_ERROR)
		{
			debugLog(_T("recv Error! %d"),WSAGetLastError());
			break;
		}
		if(!Socket::SendBuf(pInfo->s2,buffer,nCount))
		{
			debugLog(_T("send Error! %d"),WSAGetLastError());
			break;
		}
	}
	Socket::Close(pInfo->s1);
	Socket::Close(pInfo->s2);
	return 0;
}

DWORD WINAPI CTunnel::TCPTunnel( LPVOID lpParameter )
{
	return CTunnel::GetInstanceRef().TCPTunnelProc(lpParameter);
}

DWORD WINAPI CTunnel::TCPTunnelProc( LPVOID lpParameter )
{
	TUNNEL_CONFIG* pTransInfo = (TUNNEL_CONFIG*)lpParameter;

	Thread t1 , t2;

	t1.Start((LPTHREAD_START_ROUTINE)TCP_C2S,pTransInfo);
	t2.Start((LPTHREAD_START_ROUTINE)TCP_S2C, pTransInfo);

	t1.WaitForEnd();
	t2.WaitForEnd();

	if (pTransInfo)
	{
		free(pTransInfo);
	}

	debugLog(_T("Tunnel thread finish!"));

	return TRUE;

	return 0;
}

bool CTunnel::Begin(int port1,int port2)
{
	infoLog(_T("Bind ports %d and %d..."),port1,port2);

	bool ret = BindTunnel(port1,port2) && WaitTunnel();
	
	if ( !ret )
	{
		errorLog(_T("Bind failed!"));
		return ret;
	}
	infoLog(_T("Listening..."));

	Thread t1 ,t2;

	t1.Start((LPTHREAD_START_ROUTINE)Tunnel,this);
	t2.Start((LPTHREAD_START_ROUTINE)Worker,this);

	t1.WaitForEnd();
	t2.WaitForEnd();

	infoLog(_T("DISCONNECT!"));

	return ret;
}


DWORD WINAPI CTunnel::CheckMgr(LPVOID lpParameter)
{
	return CTunnel::GetInstanceRef().CheckMgrProc(lpParameter);
}

DWORD WINAPI CTunnel::CheckMgrProc(LPVOID lpParameter)
{
	char buf[10] = {0};
	recv(m_sMgr,buf,10,0);

	CTunnel::GetInstanceRef().Close();
	return 0;
}