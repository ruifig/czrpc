#pragma once

namespace cz
{
namespace rpc
{

class ObjectData
{
public:

	explicit ObjectData(void* owner) : m_owner(owner)
	{
		m_data = shared(owner);
	}

	~ObjectData()
	{
		// Release our reference to the shared data
		m_data = nullptr;
		// Remove from the map (if the weak_ptr expired)
		shared(m_owner);
	}

	Any getProperty(const std::string& name) const
	{
		auto lk = m_data->lock();
		auto it = m_data->props.find(name);
		return it == m_data->props.end() ? Any() : it->second;
	}

	// Returns true if added, false if failed (already existed, and `replace` was set to false)
	bool setProperty(const std::string& name, Any val, bool replace = false)
	{
		auto lk = m_data->lock();
		auto it = m_data->props.find(name);
		if (it==m_data->props.end())
		{
			m_data->props.insert(std::pair<std::string, Any>(name, std::move(val)));
			return true;
		}
		else
		{
			if (!replace)
				return false;
			it->second = std::move(val);
			return true;
		}
	}

	template<typename T>
	bool setProperty(const std::string& name, T val, bool replace = false)
	{
		return setProperty(name, Any(std::forward<T>(val)), replace);
	}

	std::string getAuthToken()
	{
		auto lk = m_data->lock();
		return m_data->authToken;
	}

	void setAuthToken(std::string tk)
	{
		auto lk = m_data->lock();
		m_data->authToken = std::move(tk);
	}

	bool checkAuthToken(const std::string& tk) const
	{
		auto lk = m_data->lock();
		return tk == m_data->authToken;
	}

private:

	void* m_owner;
	struct SharedData
	{
		std::unique_lock<std::mutex> lock()
		{
			return std::unique_lock<std::mutex>(mtx);
		}
		mutable std::mutex mtx;
		std::unordered_map<std::string, Any> props;
		std::string authToken;
	};
	std::shared_ptr<SharedData> m_data;

	// Given a pointer, it creates the shared data for that object pointer
	// or removes the shared data from the map if the weak_ptr expired
	static std::shared_ptr<SharedData> shared(void* owner)
	{
		static std::mutex mtx;
		static std::unordered_map<void*, std::weak_ptr<SharedData>> objs;

		std::lock_guard<std::mutex> lk(mtx);
		auto it = objs.find(owner);
		if (it==objs.end())
		{
			auto p = std::make_shared<SharedData>();
			objs.insert(std::make_pair(owner, p));
			return p;
		}
		else
		{
			auto p = it->second.lock();
			if (p)
				return p;
			objs.erase(it);
			return nullptr;
		}
	}
};

}
}

