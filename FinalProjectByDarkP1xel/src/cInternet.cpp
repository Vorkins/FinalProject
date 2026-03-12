//
//  This is a Final Project file.
//	Developer: DarkP1xel <DarkP1xel@yandex.ru>
//
//  Official Thread: https://www.blast.hk/threads/60930/
//
//  Copyright (C) 2021 BlastHack Team <BlastHack.Net>. All rights reserved.
//

#include "../cInternet.hpp"

namespace {
	auto bytesToWideString(const std::string &bytes) -> std::wstring {
		return std::wstring{bytes.cbegin(), bytes.cend()};
	}

	auto readInternetHandle(class cWinAPIFuncs *pWinAPIFuncs, const HINTERNET hHandle) -> std::wstring {
		if (pWinAPIFuncs == nullptr || hHandle == nullptr) {
			return std::wstring{};
		}

		std::string payload{};
		unsigned long totalBytesRead{0};
		while (true) {
			char chunk[2048]{};
			unsigned long bytesRead{0};
			if (!pWinAPIFuncs->internetReadFile(hHandle, &chunk[0], sizeof(chunk), &bytesRead)) {
				return std::wstring{};
			}

			if (bytesRead == 0) {
				break;
			}

			payload.append(&chunk[0], &chunk[0] + bytesRead);
			totalBytesRead += bytesRead;
		}

		return totalBytesRead > 0 ? bytesToWideString(payload) : std::wstring{};
	}

	auto readWinHttpHandle(class cWinAPIFuncs *pWinAPIFuncs, const HINTERNET hHandle) -> std::wstring {
		if (pWinAPIFuncs == nullptr || hHandle == nullptr) {
			return std::wstring{};
		}

		std::string payload{};
		while (true) {
			unsigned __int32 availableBytes{0};
			if (!pWinAPIFuncs->winHttpQueryDataAvailable(hHandle, &availableBytes)) {
				return std::wstring{};
			}

			if (availableBytes == 0) {
				break;
			}

			std::string chunk(availableBytes, '\0');
			unsigned __int32 bytesRead{0};
			if (!pWinAPIFuncs->winHttpReadData(hHandle, &chunk[0], availableBytes, &bytesRead)) {
				return std::wstring{};
			}

			if (bytesRead == 0) {
				break;
			}

			payload.append(chunk.cbegin(), chunk.cbegin() + bytesRead);
		}

		return bytesToWideString(payload);
	}
}

cInternet::cInternet(class cWinAPIFuncs *pWinAPIFuncs) {
	this->pWinAPIFuncs = pWinAPIFuncs;

	if (this->pWinAPIFuncs != nullptr) {
		WSADATA wsaData{};
		this->pWinAPIFuncs->wSAStartup(0x514, &wsaData);
	}
}


auto cInternet::sockStrToAddrW(const __int16 i16Af, const wchar_t *pURL, const unsigned __int16 ui16Port, sockaddr_in *pServerAddrIn) const -> bool {
	if (this->pWinAPIFuncs == nullptr || pURL == nullptr || pServerAddrIn == nullptr) {
		return false;
	}

	in_addr inAddr{};
	unsigned __int16 ui16PortBytes{0};
	if (this->pWinAPIFuncs->rtlIpv4StringToAddressExW(pURL, 0, &inAddr, &ui16PortBytes) == 0) {
		pServerAddrIn->sin_family = i16Af;
		pServerAddrIn->sin_port = this->pWinAPIFuncs->htons(ui16Port);
		pServerAddrIn->sin_addr = inAddr;
		return true;
	}

	return false;
}


auto cInternet::sendToW(const SOCKET socSocket, sockaddr_in *pAddrIn, const wchar_t *pData) const -> void {
	if (this->pWinAPIFuncs == nullptr || pAddrIn == nullptr || pData == nullptr) {
		return;
	}

	this->pWinAPIFuncs->sendto(socSocket, reinterpret_cast<const char *>(pData), static_cast<__int32>(wcslen(pData) * sizeof(wchar_t)), 0, reinterpret_cast<const sockaddr *>(pAddrIn), sizeof(sockaddr_in));
}


auto cInternet::recvFromW(const SOCKET socSocket, sockaddr_in *pAddrIn, const unsigned __int16 ui16RecvSize, __int32 *pTotalRecv) const -> std::wstring {
	if (this->pWinAPIFuncs == nullptr || pAddrIn == nullptr || pTotalRecv == nullptr || ui16RecvSize == 0) {
		return std::wstring{};
	}

	std::string recvBuffer(ui16RecvSize, '\0');
	__int32 i32AddrInSize{sizeof(sockaddr_in)};
	*pTotalRecv = this->pWinAPIFuncs->recvfrom(socSocket, &recvBuffer[0], ui16RecvSize, 0, reinterpret_cast<sockaddr *>(pAddrIn), &i32AddrInSize);
	if (*pTotalRecv <= 0) {
		return std::wstring{};
	}

	recvBuffer.resize(static_cast<std::size_t>(*pTotalRecv));
	return bytesToWideString(recvBuffer);
}


auto cInternet::sendInetW(const wchar_t *pUserAgent, const bool bHTTPS, const wchar_t *pURL, const unsigned __int16 ui16Port, const wchar_t *pType, const wchar_t *pParams, const wchar_t *pHeaders, const unsigned __int32 ui32HeadersLen) const -> std::wstring {
	if (this->pWinAPIFuncs == nullptr || pUserAgent == nullptr || pURL == nullptr || pType == nullptr) {
		return std::wstring{};
	}

	const unsigned __int32 requestFlags{
		!bHTTPS
			? INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID | INTERNET_FLAG_NO_AUTH
			: INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID | INTERNET_FLAG_NO_AUTH | INTERNET_FLAG_SECURE
	};

	const HINTERNET hInternetSession{this->pWinAPIFuncs->internetOpenW(pUserAgent, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0)};
	if (hInternetSession == nullptr) {
		return std::wstring{};
	}

	const HINTERNET hInternetConnect{this->pWinAPIFuncs->internetConnectW(hInternetSession, pURL, ui16Port, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0)};
	if (hInternetConnect == nullptr) {
		this->pWinAPIFuncs->internetCloseHandle(hInternetSession);
		return std::wstring{};
	}

	const HINTERNET hInternetRequest{this->pWinAPIFuncs->httpOpenRequestW(hInternetConnect, pType, pParams, L"HTTP/1.1", nullptr, nullptr, requestFlags, 0)};
	if (hInternetRequest == nullptr) {
		this->pWinAPIFuncs->internetCloseHandle(hInternetConnect);
		this->pWinAPIFuncs->internetCloseHandle(hInternetSession);
		return std::wstring{};
	}

	std::wstring response{};
	if (this->pWinAPIFuncs->httpSendRequestW(hInternetRequest, pHeaders, ui32HeadersLen, nullptr, 0)) {
		response = readInternetHandle(this->pWinAPIFuncs, hInternetRequest);
	}

	this->pWinAPIFuncs->internetCloseHandle(hInternetRequest);
	this->pWinAPIFuncs->internetCloseHandle(hInternetConnect);
	this->pWinAPIFuncs->internetCloseHandle(hInternetSession);
	return response;
}


auto cInternet::sendHttpW(const wchar_t *pUserAgent, const bool bHTTPS, const wchar_t *pURL, const unsigned __int16 ui16Port, const wchar_t *pType, const wchar_t *pParams, const wchar_t *pHeaders, const unsigned __int32 ui32HeadersLen) const -> std::wstring {
	if (this->pWinAPIFuncs == nullptr || pUserAgent == nullptr || pURL == nullptr || pType == nullptr) {
		return std::wstring{};
	}

	const HINTERNET hInternetSession{this->pWinAPIFuncs->winHttpOpen(pUserAgent, 4, nullptr, nullptr, 0)};
	if (hInternetSession == nullptr) {
		return std::wstring{};
	}

	const HINTERNET hInternetConnect{this->pWinAPIFuncs->winHttpConnect(hInternetSession, pURL, ui16Port, 0)};
	if (hInternetConnect == nullptr) {
		this->pWinAPIFuncs->winHttpCloseHandle(hInternetSession);
		return std::wstring{};
	}

	const wchar_t *pAcceptTypes[5]{L"text/plain", L"text/html", L"text/xml", L"*/*", nullptr};
	const HINTERNET hInternetRequest{this->pWinAPIFuncs->winHttpOpenRequest(hInternetConnect, pType, pParams, L"HTTP/1.1", nullptr, pAcceptTypes, !bHTTPS ? 0x00000000 : 0x00800000)};
	if (hInternetRequest == nullptr) {
		this->pWinAPIFuncs->winHttpCloseHandle(hInternetConnect);
		this->pWinAPIFuncs->winHttpCloseHandle(hInternetSession);
		return std::wstring{};
	}

	unsigned __int32 ui32RequestOptions[2]{
		0x00000004 | 0x00000001,
		0x00000001 | 0x00000002
	};

	std::wstring response{};
	if (this->pWinAPIFuncs->winHttpSetOption(hInternetRequest, 63, &ui32RequestOptions[0], sizeof(unsigned __int32)) &&
		this->pWinAPIFuncs->winHttpSetOption(hInternetRequest, 118, &ui32RequestOptions[1], sizeof(unsigned __int32)) &&
		this->pWinAPIFuncs->winHttpSendRequest(hInternetRequest, pHeaders, ui32HeadersLen, nullptr, 0, 0, 0) &&
		this->pWinAPIFuncs->winHttpReceiveResponse(hInternetRequest, nullptr)) {
		response = readWinHttpHandle(this->pWinAPIFuncs, hInternetRequest);
	}

	this->pWinAPIFuncs->winHttpCloseHandle(hInternetRequest);
	this->pWinAPIFuncs->winHttpCloseHandle(hInternetConnect);
	this->pWinAPIFuncs->winHttpCloseHandle(hInternetSession);
	return response;
}


auto cInternet::readURLInetW(const wchar_t *pUserAgent, const bool bHTTPS, const wchar_t *pURL) const -> std::wstring {
	if (this->pWinAPIFuncs == nullptr || pUserAgent == nullptr || pURL == nullptr) {
		return std::wstring{};
	}

	const unsigned __int32 requestFlags{
		!bHTTPS
			? INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID | INTERNET_FLAG_NO_AUTH
			: INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID | INTERNET_FLAG_IGNORE_CERT_DATE_INVALID | INTERNET_FLAG_NO_AUTH | INTERNET_FLAG_SECURE
	};

	const HINTERNET hInternetSession{this->pWinAPIFuncs->internetOpenW(pUserAgent, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0)};
	if (hInternetSession == nullptr) {
		return std::wstring{};
	}

	const HINTERNET hInternetOpen{this->pWinAPIFuncs->internetOpenUrlW(hInternetSession, pURL, nullptr, 0, requestFlags, 0)};
	if (hInternetOpen == nullptr) {
		this->pWinAPIFuncs->internetCloseHandle(hInternetSession);
		return std::wstring{};
	}

	const std::wstring response{readInternetHandle(this->pWinAPIFuncs, hInternetOpen)};
	this->pWinAPIFuncs->internetCloseHandle(hInternetOpen);
	this->pWinAPIFuncs->internetCloseHandle(hInternetSession);
	return response;
}


auto cInternet::readURLHttpW(const wchar_t *pUserAgent, const bool bHTTPS, const wchar_t *pURL) const -> std::wstring {
	const unsigned __int16 ui16Port{bHTTPS ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT};
	return this->sendHttpW(pUserAgent, bHTTPS, pURL, ui16Port, L"GET", nullptr, nullptr, 0);
}


cInternet::~cInternet(void) {
	if (this->pWinAPIFuncs != nullptr) {
		this->pWinAPIFuncs->wSACleanup();
		this->pWinAPIFuncs = nullptr;
	}
}
