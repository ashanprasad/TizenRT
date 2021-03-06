/* ****************************************************************
 *
 * Copyright 2018 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************/

#include <debug.h>
#include "MediaWorker.h"

namespace media {
	

MediaWorker::MediaWorker() : mStacksize(PTHREAD_STACK_DEFAULT), mThreadName("MediaWorker"), mIsRunning(false), mRefCnt(0)
{
	medvdbg("MediaWorker::MediaWorker()\n");
}
MediaWorker::~MediaWorker()
{
	medvdbg("MediaWorker::~MediaWorker()\n");
}

void MediaWorker::startWorker()
{
	std::unique_lock<std::mutex> lock(mRefMtx);
	++mRefCnt;
	medvdbg("MediaWorker::startWorker() - increase RefCnt : %d\n", mRefCnt);
	if (mRefCnt == 1) {
		int ret;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, mStacksize);
		mIsRunning = true;
		ret = pthread_create(&mWorkerThread, &attr, (pthread_startroutine_t)&MediaWorker::mediaLooper, this);
		if (ret != OK) {
			medvdbg("Fail to create worker thread, return value : %d\n", ret);
			--mRefCnt;
			return;
		}
		pthread_setname_np(mWorkerThread, mThreadName);
	}
}

void MediaWorker::stopWorker()
{
	std::unique_lock<std::mutex> lock(mRefMtx);
	if (mRefCnt > 0) {
		--mRefCnt;
	}
	medvdbg("MediaWorker::stopWorker() - decrease RefCnt : %d\n", mRefCnt);
	if (mRefCnt <= 0) {
		std::atomic<bool> &refBool = mIsRunning;
		mWorkerQueue.enQueue([&refBool]() {
			refBool = false;
		});
		pthread_join(mWorkerThread, NULL);
		medvdbg("MediaWorker::stopWorker() - mWorkerthread exited\n");
	}
}

std::function<void()> MediaWorker::deQueue()
{
	return mWorkerQueue.deQueue();
}

bool MediaWorker::processLoop()
{
	return false;
}

void *MediaWorker::mediaLooper()
{
	medvdbg("MediaWorker : mediaLooper\n");

	while (mIsRunning) {
		while (processLoop() && mWorkerQueue.isEmpty());

		std::function<void()> run = deQueue();
		medvdbg("MediaWorker : deQueue\n");
		if (run != nullptr) {
			run();
		}
	}
	return NULL;
}

bool MediaWorker::isAlive()
{
	std::unique_lock<std::mutex> lock(mRefMtx);
	return mRefCnt == 0 ? false : true ;
}
} // namespace media
