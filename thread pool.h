#include <vector>
#include <queue>
#include <mutex>
#include <future>
#include <thread>
#include <utility>
#include <functional>
#include <shared_mutex>

//����һ����ȫ���� 
template <typename T>
class SafeQueue{
private:
	std::queue<T> m_queue;//������� 
	std::shared_mutex m_mutex;
public:
	SafeQueue() {}
	SafeQueue(SafeQueue &&other) {}
	~SafeQueue() {}
	//���ض����Ƿ�Ϊ��
	bool empty(){
		std::shared_lock<std::shared_mutex> lock(m_mutex);
		return m_queue.empty(); 
	} 
	//���ض��д�С
	int size(){ 
		std::shared_lock<std::shared_mutex> lock(m_mutex);
		return m_queue.size();
	}
	//�������Ԫ��
	void push(T &t){
		std::unique_lock<std::shared_mutex> lock(m_mutex);
		m_queue.emplace(t);
	} 
	//����ȡ��Ԫ��
	bool pop(T &t){
		std::unique_lock<std::shared_mutex> lock(m_mutex);
		if(m_queue.empty()) return false;
		t=std::move(m_queue.front());//ȡ������Ԫ�أ����ض���Ԫ��ֵ����������ֵ���� 
		m_queue.pop();//��������Ԫ�� 
		return true;
	}
};

class ThreadPool{
private:
	class ThreadWorker{
	private:
		ThreadPool *m_pool;//�����̳߳�
	public:
		ThreadWorker(ThreadPool * pool) : m_pool(pool){
		} 
		//����()����
		//��()�������дӶ�����ȡ�����ִ�� 
		void operator()(){
			std::function<void ()> func;
			bool flag=false;//�Ƿ�Ӷ�����ȡ��Ԫ�� 
			while(!m_pool->m_shutdown){
				{
					std::unique_lock<std::mutex> lock(m_pool -> m_mutex);
					//�������Ϊ�յĻ���������ǰ���� 
					if(m_pool -> m_queue.empty()){
						m_pool -> m_lock.wait(lock);
					}
					//ȡ�������е�Ԫ�� 
					flag = m_pool -> m_queue.pop(func);
				}
				// ����ɹ�ȡ����ִ�й������� 
				if(flag) func();
			}
		}
	};
	
	bool m_shutdown;//�̳߳��Ƿ�ر�
	SafeQueue<std::function<void()>> m_queue;
	std::vector<std::thread> m_threads;
	std::mutex m_mutex;
	std::condition_variable m_lock;
public:
	ThreadPool(const int n = 4) : m_threads(n),m_shutdown(false){
		for(auto & i : m_threads)
			i = std::thread(ThreadWorker(this));
	}
	ThreadPool(const ThreadPool &) = delete;
	ThreadPool(ThreadPool &&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
	
	template <typename F,typename ... Args>
	auto submit(F && f, Args && ... args) -> std::future<decltype(f(args...))>
	{
		std::function<decltype(f(args ...))()> func = std::bind(std::forward<F>(f),std::forward<Args>(args)...);
		auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args ...))()>>(func);
		std::function<void()> warpper_func = [task_ptr](){
			(*task_ptr)();
		};
		m_queue.push(warpper_func);
		m_lock.notify_one();
		return task_ptr->get_future();
	}
	
	~ThreadPool(){
		m_shutdown = true;
		//�������н��� 
		m_lock.notify_all();
		
		for(int i=0;i<m_threads.size();i++){
			if(m_threads[i].joinable())
				m_threads[i].join();
		} 
	}
};
