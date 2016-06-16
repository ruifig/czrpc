#pragma once

namespace cz
{
namespace rpc
{

class Properties
{
public:

	explicit Properties(void* owner) : m_owner(owner)
	{
		m_data = shared(owner);
	}

	~Properties()
	{
		// Release our reference to the shared data
		m_data = nullptr;
		// Remove from the map (if the weak_ptr expired)
		shared(m_owner);
	}

	Any getProperty(const std::string& name) const
	{
		std::lock_guard<std::mutex> lk(m_data->mtx);
		auto it = m_data->props.find(name);
		return it == m_data->props.end() ? Any() : it->second;
	}

	// Returns true if added, false if failed (already existed, and `replace` was set to false)
	bool setProperty(const std::string& name, Any val, bool replace = false)
	{
		std::lock_guard<std::mutex> lk(m_data->mtx);
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

private:

	void* m_owner;
	struct SharedData
	{
		mutable std::mutex mtx;
		std::unordered_map<std::string, Any> props;
	};
	std::shared_ptr<SharedData> m_data;

	// Given a pointer, it creates a new Properties object shared pointer mapped
	// to that pointer, or removes the Properties object from the map if the
	// weak_ptr is empty
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

