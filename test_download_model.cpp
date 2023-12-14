/*
这段代码在jetson nano 平台会出现公网下载失败的情况，监听的curl句柄一直不返回，也不增加新增的下载内容。
*/


#ifndef _MULTI_DOWNLOAD_H_
#define _MULTI_DOWNLOAD_H_

#include <iomanip>
#include <fstream>
#include <iostream>
#include <functional>
#include <string>
#include <deque>
#include <map>
#include <chrono>
#include <string.h>
#include <thread>
#include <mutex>
#ifndef _WIN32
#include <unistd.h>
#endif

#include "curl/curl.h"

// ����E_RESPONSE_CODEΪHTTP��response code�����
enum DownloadResult {
	E_OK,
	E_MEMORY,
	E_TIMEOUT,
	E_DOWNLOADFAIL,

	E_RESPONSE_CODE = 399,
	E_FORBIDDEN = 403,
	E_NOTFOUND = 404,
	E_GATEWAYTIMEOUT = 502,
};
enum CallbackType {
	RESULT,
	FILESIZE,
	CONTENT
};
struct CallbackData {
	CallbackType type;
	union {
		size_t fileSize;
		struct {
			char* data;
			size_t size;
		};
		DownloadResult result;
	};
};

template<typename LockType = std::mutex>
class MultiDownload {
public:
	typedef std::function<void(const char* fileId, const char* url, const CallbackData& callbackData)> DownloadCallback;
	typedef std::function<curl_slist*(curl_slist* header)> HeaderCallback;
	MultiDownload(size_t maxConcurrency)
	:m_stop(false)
	,m_routine(std::bind(&MultiDownload::downloadRoutine, this))
	,m_joinStop(false)
	,m_maxConcurrency(maxConcurrency)
	{
		m_curlm = curl_multi_init();
	}
	~MultiDownload() {
		m_stop = true;
		if (m_curlm) {
			curl_multi_cleanup(m_curlm);
		}
	}
	bool addPost(const char* fileId, const char* url, const char* data, size_t length, int timeout_ms, DownloadCallback cb, HeaderCallback hcb = nullptr) {
		// ֹͣ�����в��ٽ����µ���������
		if (m_joinStop || m_stop) {
			return false;
		}
		DownloadContext ctx;
		ctx.fileId = fileId;
		ctx.url = url;
		ctx.cb = cb;
		ctx.hcb = hcb;
		ctx.timeout_ms = timeout_ms;
		ctx.isPost = true;
		ctx.context.assign(data, length);
		m_downloadQueueLock.lock();
		m_downloadQueue.push_back(ctx);
		m_downloadQueueLock.unlock();
		return true;
	}
	size_t QueueSize()
	{
		return m_downloadQueue.size();
	}
	bool addDownload(const char* fileId, const char* url, int timeout_ms, DownloadCallback cb) {
		// ֹͣ�����в��ٽ����µ���������
		if (m_joinStop || m_stop) {
			return false;
		}
		DownloadContext ctx;
		ctx.fileId = fileId;
		ctx.url = url;
		ctx.cb = cb;
		ctx.timeout_ms = timeout_ms;
		ctx.isPost = false;
		m_downloadQueueLock.lock();
		m_downloadQueue.push_back(ctx);
		m_downloadQueueLock.unlock();
		return true;
	}

	void join() {
		m_joinStop = true;
		m_stop = true;
		m_routine.join();
	}
private:
	struct DownloadContext {
		std::string fileId;
		std::string url;
		DownloadCallback cb;
		HeaderCallback hcb;

		int timeout_ms;
		bool isPost;
		std::string context;
	};
	class DownloadInstance {
	public:
		DownloadContext ctx;
		CURL* curl;
		MultiDownload<LockType>* self;
		curl_slist * headerList;
		bool init() {
			bool ret = false;
			curl = curl_easy_init();
			if (curl) {
				curl_easy_setopt(curl, CURLOPT_URL, ctx.url.c_str());
				curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, ctx.timeout_ms);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, DownloadInstance::writeFunction);
				curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
				curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, DownloadInstance::headerFunction);
				if (ctx.isPost) {
					headerList = curl_slist_append(NULL, "Expect:");
					if (ctx.hcb)
					{
						headerList = ctx.hcb(headerList);
					}
					curl_easy_setopt(curl, CURLOPT_POST, 1);
					curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ctx.context.c_str());
					curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, ctx.context.length());
					curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
					
				}
				ret = true;
			}
			if (!ret && curl) {
				curl_easy_cleanup(curl);
				curl = NULL;
			}
			return ret;
		}
		void cleanup() {
			if (curl) {
				curl_easy_cleanup(curl);
				curl = NULL;
			}
			if (headerList)
			{
				curl_slist_free_all(headerList);
				headerList = 0;
			}
		}
		DownloadInstance() : curl(NULL), self(NULL), headerList(NULL) {}
		DownloadInstance(DownloadInstance& rhs) {
			swap(rhs);
		}
		~DownloadInstance() {
			cleanup();
		}
		void swap(DownloadInstance& rhs) noexcept {
			ctx = rhs.ctx;
			curl = rhs.curl;
			self = rhs.self;
			rhs.curl = NULL;
		}
	private:
		const DownloadInstance& operator=(const DownloadInstance&);
		static size_t writeFunction(char *ptr, size_t size, size_t nmemb, void *userdata) {
			CallbackData callbackData;
			callbackData.type = CONTENT;
			callbackData.data = ptr;
			callbackData.size = size*nmemb;
			DownloadInstance*inst = static_cast<DownloadInstance*>(userdata);
			inst->self->safeCallback(*inst, callbackData);
			return size*nmemb;
		}
#ifdef _WIN32
#define strncasecmp _strnicmp
#endif

		static size_t headerFunction(char *ptr, size_t size, size_t nmemb, void *userdata) {
			CallbackData callbackData;
			callbackData.type = FILESIZE;
			static size_t length = strlen("Content-Length");
			if (strncasecmp(ptr, "Content-Length", length) == 0) {
				size_t idx = length;
				while (idx < size*nmemb && ptr[idx] != ':') {
					idx++;
				}
				callbackData.fileSize = atoi(ptr + idx + 1);
				DownloadInstance*inst = static_cast<DownloadInstance*>(userdata);
				inst->self->safeCallback(*inst, callbackData);
			}
			return size*nmemb;
		}

#undef strncasecmp
	};

	void downloadRoutine() {
		std::map<CURL*, DownloadInstance*> downloading;
		DownloadInstance* newDownload = NULL;
		int runningHandle = 0;
		CURLMcode retCurl;
		CallbackData callbackData;
		while(true) {
			// ǿ�ƹر�
			if (!m_joinStop && m_stop) {
				break;
			}
			//������ɷ�����
			if (m_joinStop && m_stop && downloading.empty() && m_downloadQueue.empty()) {
				break;
			}
			newDownload = NULL;
			if (downloading.size() < m_maxConcurrency && !m_downloadQueue.empty()) {
				m_downloadQueueLock.lock();
				newDownload = new DownloadInstance;
				newDownload->self = this;
				newDownload->ctx = m_downloadQueue.front();
				m_downloadQueue.pop_front();
				m_downloadQueueLock.unlock();
			}
			if (newDownload) {
				if (newDownload->init() && curl_multi_add_handle(m_curlm, newDownload->curl) == CURLM_OK) {
					downloading[newDownload->curl] = newDownload;
				} else {
					callbackData.type = RESULT;
					callbackData.result = E_MEMORY;
					safeCallback(*newDownload, callbackData);
					delete newDownload;
				}
			}
			if (downloading.empty()) {
				yield();
				continue;
			}
			int ret = 0;
			while(CURLM_CALL_MULTI_PERFORM == curl_multi_perform(m_curlm, &runningHandle)) {}
			retCurl = curl_multi_wait(m_curlm, NULL, 0, 50, &ret);
			if (retCurl != CURLM_OK) {
				//TODO�����������־
				std::cout << __FILE__ << ":" << __LINE__ << std::endl;
			}
			if (ret != 0) {
				while(CURLM_CALL_MULTI_PERFORM == curl_multi_perform(m_curlm, &runningHandle)) {}
			}

			int msgsLeft;
			CURLMsg *msg;
			while((msg = curl_multi_info_read(m_curlm, &msgsLeft))) {
				if (CURLMSG_DONE == msg->msg) {
					curl_multi_remove_handle(m_curlm, msg->easy_handle);
					auto it = downloading.find(msg->easy_handle);
					if (it == downloading.end()) {
						//TODO�����������־
						std::cout << __FILE__ << ":" << __LINE__ << std::endl;
					} else {
						DownloadInstance* inst = it->second;
						downloading.erase(it);
						if (CURLE_OK != msg->data.result) {
							callbackData.type = RESULT;
							callbackData.result = translateCURLCode(msg->data.result);
							safeCallback(*inst, callbackData);
						} else {
							int responseCode;
							curl_easy_getinfo(inst->curl, CURLINFO_RESPONSE_CODE, &responseCode);
							if (responseCode > 300) {
								callbackData.type = RESULT;
								callbackData.result = translateResponseCode(responseCode);
								safeCallback(*inst, callbackData);
							} else {
								callbackData.type = RESULT;
								callbackData.result = E_OK;
								safeCallback(*inst, callbackData);
							}
						}
						if (inst) {
							delete inst;
						}
					}
				}
			}
		}
	}
	friend class DownloadInstance;
	void safeCallback(const DownloadInstance& inst, const CallbackData& callbackData) const {
		if (inst.ctx.cb) {
			inst.ctx.cb(inst.ctx.fileId.c_str(), inst.ctx.url.c_str(), callbackData);
		}
	}

	static void yield() {
#ifdef _WIN32
		std::this_thread::yield();
#else
		usleep(0);
#endif
	}
	static DownloadResult translateCURLCode(CURLcode code) {
		switch(code) {
		case CURLE_OK:
			return E_OK;
		case CURLE_OPERATION_TIMEDOUT:
			return E_TIMEOUT;
		default:
			return E_DOWNLOADFAIL;
		}
	}
	static DownloadResult translateResponseCode(int code) {
		switch(code) {
		case 200:
			return E_OK;
		case 403:
			return E_FORBIDDEN;
		case 404:
			return E_NOTFOUND;
		case 502:
			return E_GATEWAYTIMEOUT;
		default:
			return E_RESPONSE_CODE;
		}
	}

	CURLM* m_curlm;
	std::deque<DownloadContext> m_downloadQueue;
	LockType m_downloadQueueLock;
	volatile bool m_stop;
	std::thread m_routine;
	volatile bool m_joinStop;
	const size_t m_maxConcurrency;
};

#endif







///================================================================///


#define LOG_TRACE std::cout

static bool threadsWorking_ = true;

bool DownloadModel(const std::string &url, const std::string &savePath)
{
    if (url.empty())
    {
        LOG_TRACE << "ERROR: " << "empty download url" << std::endl;
        return false;
    }

    MultiDownload<std::mutex> download(1);
    std::ofstream oModelZip(savePath, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!oModelZip.is_open())
    {
        LOG_TRACE << "ERROR: " << "open file to write failed: " << savePath << std::endl;
        return false;
    }

    size_t totalSize;
    volatile double downloadProgress = 0.0;
    volatile double lastPrintProgress = -99.9;
    volatile bool bFinished = false;

    bool bDownloadOk = false;
    bDownloadOk = download.addDownload(
        savePath.c_str(), url.c_str(), 0,
        [&](const char *fileId, const char *url, const CallbackData &cbdata) mutable {
            if (FILESIZE == cbdata.type)
            {
                totalSize = cbdata.fileSize;
                LOG_TRACE << "NOTICE: " << "Download header ok, filesize: " << totalSize << ", url: " << url << std::endl;
                return;
            }
            else if (CONTENT == cbdata.type)
            {
                oModelZip.write(cbdata.data, cbdata.size);
                downloadProgress += 100.0 * cbdata.size / totalSize;
                // if (downloadProgress - lastPrintProgress > 2.0)
                // {
                    lastPrintProgress = downloadProgress;
                //     LOG_TRACE << "INFO: " << "Download progress: " << lastPrintProgress << " %" << std::endl;
                // }
                LOG_TRACE << "\rINFO: " << "Download progress: " << std::setw(10) << downloadProgress << " %" << std::flush;
                return;
            }
            else if (RESULT == cbdata.type)
            {
                bFinished = true;
                if (E_OK != cbdata.result)
                {
                    LOG_TRACE << "ERROR: "
                              << "Download error, url: " << url << ", result=" << cbdata.result
                              << ", total size: " << totalSize << ", progress: " << downloadProgress << std::endl;
                    bDownloadOk = false;
                    return;
                }
                bDownloadOk = true;
                LOG_TRACE << "NOTICE: "
                          << "Download succeed, url: " << url << ", result" << cbdata.result
                          << ", size: " << totalSize << ", progress: " << downloadProgress << std::endl;
                return;
            }
            else
            {
                bFinished = true;
                bDownloadOk = false;
                LOG_TRACE << "ERROR: "
                          << "Download error, unkown cbdata type: " << cbdata.type
                          << ", url: " << url << ", progress: " << downloadProgress << std::endl;
                return;
            }
        });

    while (!bFinished)
    {
        if (!threadsWorking_)
        {
            return bDownloadOk;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
        download.join();

    return bDownloadOk;
}


#include <cassert>

int main(int argc, char* argv[])
{
    assert(argc == 2);
    return DownloadModel(argv[1], "test_download_model.data");
}