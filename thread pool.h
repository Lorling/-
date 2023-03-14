#include <vector>
#include <queue>
#include <mutex>
#include <future>
#include <thread>
#include <utility>
#include <functional>

template <typename T>
class SafeQueue{
private:
	std::queue<T> m_queue;
	
	std::mutex m_mutex;
public:
	SafeQueue() {};
	SafeQueue(SafeQueue &&other) {};
	~SafeQueue() {};
	
	bool empty(){
		std::unique_lock<std::mutex> lock(m_mutex);
		
		return m_queue.empty();
	}
	
	int size(){
		std::unique_lock<std::mutex> lock(m_mutex);
		
		return m_queue.size();
	}
	
	void enqueue(T &t){
		std::unique_lock<std::mutex> lock(m_mutex);
		
		m_queue.emplace(t);
	}
	
	bool dequeue(T &t){
		std::unique_lock<std::mutex> lock(m_mutex);
		
		if(m_queue.empty())
			return false;
		
		t = std::move(m_queue.front());
		m_queue.pop();
		
		return true;
	}
};

class ThreadPool{
private:
	class ThreadWorker{
	private:
		int m_id;//工作id
		
		ThreadPool *m_pool;//所属线程池
	public:
		ThreadWorker(ThreadPool * pool, const int id)
		{
			m_pool = pool;
			m_id = id;
		} 
		
		//重载()操作
		//让()操作进行从队列中取任务和执行 
		void operator()(){
			std::function<void ()> func;
			
			bool dequeued;//是否从队列中取出元素 
			
			while(!m_pool->m_shutdown){
				{
					std::unique_lock<std::mutex> lock(m_pool -> m_conditional_mutex);
					//如果队列为空的话，阻塞当前进程 
					if(m_pool -> m_queue.empty()){
						m_pool -> m_conditional_lock.wait(lock);
					}
					
					//取出队列中的元素 
					dequeued = m_pool -> m_queue.dequeue(func);
				}
				
				// 如果成功取出则执行工作函数 
				if(dequeued) func();
			}
		}
	};
	
	bool m_shutdown;//线程池是否关闭
	
	SafeQueue<std::function<void()>> m_queue;
	
	std::vector<std::thread> m_threads;
	
	std::mutex m_conditional_mutex;
	
	std::condition_variable m_conditional_lock;
public:
	ThreadPool(const int n_threads = 4){
		//创建一个大小为n_threads的thread数组 
		m_threads = std::vector<std::thread> (n_threads);
		m_shutdown=false;
	}
	
	void init(){
		for(int i=0; i<m_threads.size();i++){
			m_threads[i] = std::thread(ThreadWorker(this,i)) ;
		}
	} 
	
	void shutdown(){
		m_shutdown = true;
		//唤醒所有进程 
		m_conditional_lock.notify_all();
		
		for(int i=0;i<m_threads.size();i++){
			if(m_threads[i].joinable())
				m_threads[i].join();
		} 
	}
	
	template <typename F,typename ... Args>
	auto submit(F && f, Args && ... args) -> std::future<decltype(f(args...))>
	{
		std::function<decltype(f(args ...))()> func = std::bind(std::forward<F>(f),std::forward<Args>(args)...);
		
		auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args ...))()>>(func);
		
		std::function<void()> warpper_func = [task_ptr](){
			(*task_ptr)();
		};
		
		m_queue.enqueue(warpper_func);
		
		m_conditional_lock.notify_one();
		
		return task_ptr->get_future();
	}
};
