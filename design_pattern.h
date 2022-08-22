#pragma once

#if __cplusplus >= 201703L
#include <any>
#endif // __cplusplus >= 201703L

#include <atomic>
#include <deque>
#include <exception>
#include <functional>
#include <iomanip>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <type_traits>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "Dbghelp.lib")
#else
#include <cxxabi.h>
#endif
#include <iostream>

#define NESTED_LESS(l, r) \
if (l < r) return true; \
if (r < l) return false

class noncopyable
{
    noncopyable(const noncopyable &) = delete;
    noncopyable& operator=(const noncopyable &) = delete;

  protected:
    noncopyable() = default;
    virtual ~noncopyable() = default;
};

template <typename T> 
class Singleton : noncopyable
{
  public:
    static T &Instance()
    {
        static T instance;
        return instance;
    }

  protected:
    Singleton()=default;
    ~Singleton()=default;
};


inline std::string GetClearName(const char *name)
{
#ifdef _WIN32
    char buff[512]{0};
    auto ret = UnDecorateSymbolName(name, buff, sizeof(buff), UNDNAME_COMPLETE);
    if (ret == 0)
    {
        std::cout << "LastError: " << GetLastError() << std::endl;
        return std::string(name);
    }
    return std::string(buff);
#else
    int status = -1;
    char *clear_name = abi::__cxa_demangle(name, NULL, NULL, &status);
    const char *demangle_name = (status == 0) ? clear_name : name;
    std::string ret_val(demangle_name);
    free(clear_name);
    return ret_val;
#endif
}

/// 工厂模板，相同类型的key值，不同的构造函数参数列表，
/// 会由不同的map管理，更容易创建出多个不同类型的单例工厂实例，但NewProduct时无需类型转换
template <typename Product_t, typename ProductName_t = std::string, typename... Args>
class Factory
{
  public:
    using ProduceFuncType = std::function<std::unique_ptr<Product_t>(Args &&...)>;

    static Factory &Instance()
    {
        static Factory instance;
        return instance;
    }

    template <typename ProductImpl_t>
    void RegistProduct(const ProductName_t &name)
    {
        const auto &product = std::make_pair(
            name, [](Args &&...args) { return std::make_unique<ProductImpl_t>(std::forward<Args>(args)...); });

        if (producers_.insert(product).second != true)
        {
            throw std::runtime_error("product name already registed");
        }
    }

	void UnregisterProduct(const ProductName_t &name)
	{
		producers_.erase(name);
	}

    std::unique_ptr<Product_t> NewProduct(const ProductName_t &name, Args &&...args)
    {
        auto it = producers_.find(name);
        if (it == producers_.end())
        {
            throw std::runtime_error("product not regist yet");
        }

        return it->second(std::forward<Args>(args)...);
    }

    // Just for debugging purposes.
    std::string AllRegistedProducts()
    {
        std::ostringstream oss;
        for (auto & v : producers_)
        {
            oss << std::left << std::setw(10) << v.first << " [" << GetClearName(typeid(ProductName_t).name())
                << "] : [" << GetClearName(typeid(ProduceFuncType).name()) << "]\n";
        }
        return oss.str();
    }
	
  private:
    Factory() = default;
    Factory(const Factory &) = delete;
    
    std::map<ProductName_t, ProduceFuncType> producers_;
};


#if __cplusplus >= 201703L

/// 工厂偏特化版本，相同类型的key值由一个map统一管理, NewProduct时由 std::any_cast
/// 决定构造参数类型，只有key类型不同时才会出现不同的工厂管理map实例。
template <typename Product_t, typename ProductName_t>
class Factory<Product_t, ProductName_t>
{
  public:
    static Factory &Instance()
    {
        static Factory instance;
        return instance;
    }

    template <typename ProductImpl_t, typename... Args>
    void RegistProduct(const ProductName_t &name)
    {
        using ProduceFuncType = std::function<std::unique_ptr<Product_t>(Args && ...)>;

        std::any produce_func = std::make_any<ProduceFuncType>(
            [](Args &&...args) { return std::make_unique<ProductImpl_t>(std::forward<Args>(args)...); });

        const auto &product = std::make_pair(name, produce_func);
        if (producers_.insert(product).second != true)
        {
            throw std::runtime_error("product name already registed");
        }
    }

	void UnregisterProduct(const ProductName_t &name)
	{
		producers_.erase(name);
	}

    template <typename... Args>
    std::unique_ptr<Product_t> NewProduct(const ProductName_t &name, Args &&...args)
    {
        auto it = producers_.find(name);
        if (it == producers_.end())
        {
            throw std::runtime_error("product not regist yet");
        }

        using ProduceFuncType = std::function<std::unique_ptr<Product_t>(Args && ...)>;

        ProduceFuncType produce_func;
        std::any produce_func_registed = it->second;

        try
        {
            produce_func = std::any_cast<ProduceFuncType>(it->second);
        }
        catch (const std::exception &e)
        {
            std::ostringstream oss_err;
            oss_err << "==================================================\n" << e.what() << '\n';
            oss_err << "registed producer raw type: " << GetClearName(typeid(produce_func_registed).name()) << '\n';
            oss_err << "registed producer real type: " << GetClearName(produce_func_registed.type().name()) << '\n';
            oss_err << "target bad_cast_func type: " << GetClearName(typeid(produce_func).name()) << '\n';
            oss_err << "==================================================";
            throw std::runtime_error(oss_err.str());
        }

        return produce_func(std::forward<Args>(args)...);
    }

    // Just for debugging purposes.
    std::string AllRegistedProducts()
    {
        std::ostringstream oss;
        for (auto & v : producers_)
        {
            oss << std::left << std::setw(10) << v.first << " [" << GetClearName(typeid(ProductName_t).name())
                << "] : [" << GetClearName(v.second.type().name()) << "]\n";
        }
        return oss.str();
    }

  private:
    Factory() = default;
    Factory(const Factory &) = default;

    std::map<ProductName_t, std::any> producers_;
};
#endif // __cplusplus >= 201703L


template<typename ListenerFunction, typename ListenerHandle = size_t>
class Listener : public noncopyable {
public:
	template<typename func>
	ListenerHandle operator+(func f) {
		auto key = atomic_fetch_add(&m_listenerKey, ListenerHandle(1));
		m_listeners[key] = f;
		return key;
	}
	template<typename func>
	void operator+=(func f) {
		auto key = atomic_fetch_add(&m_listenerKey, ListenerHandle(1));
		m_listeners[key] = f;
	}
	void operator-(ListenerHandle h) {
		m_listeners.erase(h);
	}
	template<typename ...Args>
	void notify(Args... args) {
		for (auto f : m_listeners) {
			f.second(std::forward<Args>(args)...);
		}
	}
private:
	std::atomic<ListenerHandle> m_listenerKey;
	std::map<ListenerHandle, std::function<ListenerFunction>> m_listeners;
};

template<typename ...Types>
struct Vistor;

template<typename T, typename ...Types>
struct Vistor<T, Types...> : Vistor<Types...> {
	using Vistor<Types...>::Visit;
	virtual void visit(const T&) const = 0;
	virtual ~Vistor() {}
};

template<typename T>
struct Vistor<T> {
	virtual void visit(const T&) = 0;
	virtual ~Vistor() {}
};

template<typename ExecuteFunction>
struct Command {
	template<class F, class ...BindArgs>
	void bind(F&& f, BindArgs&& ... args) {
		m_function = std::bind(f, std::forward<BindArgs&&>(args)...);
	}
	template<typename ...ExecuteArgs>
	typename std::function<ExecuteFunction>::result_type execute(ExecuteArgs&&... args) {
		return m_function(std::forward<ExecuteArgs>(args)...);
	}
private:
	std::function<ExecuteFunction>  m_function;
};

template <typename T>
struct has_reset {
private:
	template <typename U>
	static decltype(std::declval<U>().reset(), std::true_type()) test(int) {
		return std::true_type();
	}
	template <typename>
	static std::false_type test(...) {
		return std::false_type();
	}
public:
	typedef decltype(test<T>(0)) type;
};

template<typename T>
class range {
public:
	class iterator {
	public:
		iterator(const T& cur, const T& step) : _cur(cur), _step(step) {}
		T operator*() const {
			return _cur;
		}
		bool operator==(const iterator& rhs) const {
			return !(_cur != rhs);
		}
		bool operator!=(const iterator& rhs) const {
			if (_step == 0) {
				return _cur != rhs._cur;
			}
			return _step < 0 ? _cur > rhs._cur : _cur < rhs._cur;
		}
		iterator operator++() {
			_cur += _step;
			return *this;
		}
		iterator operator++(int) {
			auto ret = iterator(_cur, _step);
			_cur += _step;
			return ret;
		}
		iterator operator--() {
			_cur -= _step;
			return *this;
		}
		iterator operator--(int) {
			auto ret = iterator(_cur, _step);
			_cur -= _step;
			return ret;
		}
	private:
		T _cur;
		T _step;
	};
	range(const T& beg, const T& end, const T& step = T(1)) : _beg(beg), _end(end), _step(step) {}
	iterator begin() const {
		return iterator(_beg, _step);
	}
	iterator end() const {
		return iterator(_end, _step);
	}
private:
	const T _beg, _end, _step;
};



template<typename T, typename LockType = void, typename = typename std::enable_if<has_reset<T>::type::value>::type>
class ObjectPool : noncopyable {
public:
	template<typename ...Args>
	ObjectPool(size_t num, Args&&... args) {
		while(num--) {
			_objs.emplace_back(new T(std::forward <Args&&>(args)...));
		}
	}
	std::shared_ptr<T> get() {
		std::lock_guard<LockType> _(_lock);
		if (_objs.empty()) {
			return std::shared_ptr<T>();
		}
		auto p = std::shared_ptr<T>(_objs.front(), [&](T* ptr)
		{
			this->free(ptr);
		});
		_objs.pop_front();
		return p;
	}
	T* alloc() noexcept {
		std::lock_guard<LockType> _(_lock);
		if (_objs.empty()) {
			return nullptr;
		}
		auto p = _objs.front();
		_objs.pop_front();
		return p;
	}
	void free(T* ptr) noexcept {
		std::lock_guard<LockType> _(_lock);
		_objs.emplace_back(ptr);
	}
private:
	std::deque<T*> _objs;
	LockType _lock;
};

template<typename T>
class ObjectPool<T, void, typename std::enable_if<has_reset<T>::type::value>::type> : noncopyable {
public:
	template<typename ...Args>
	ObjectPool(size_t num, Args&&... args) {
		while(num--) {
			_objs.emplace_back(new T(std::forward<Args&&>(args)...));
		}
	}
	std::shared_ptr<T> get() {
		if (_objs.empty()) {
			return std::shared_ptr<T>();
		}
		auto p = std::shared_ptr<T>(_objs.front(), [&](T* ptr)
		{
			this->free(ptr);
		});
		_objs.pop_front();
		return p;
	}
	T* alloc() noexcept {
		if (_objs.empty()) {
			return nullptr;
		}
		auto p = _objs.front();
		_objs.pop_front();
		return p;
	}
	void free(T* ptr) noexcept {
		_objs.emplace_back(ptr);
	}
private:
	std::deque<T*> _objs;
};
