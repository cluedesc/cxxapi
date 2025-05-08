#ifdef CXXAPI_USE_REDIS_IMPL

// include once
#include <boost/redis/src.hpp>
// ...

#include <cxxapi.hxx>

namespace shared {
    bool c_redis::init(const redis_cfg_t& cfg) {
        m_cfg = cfg;

#ifdef CXXAPI_HAS_LOGGING_IMPL
        m_logger.init(
            cfg.m_cxxapi_logger.m_level,
            cfg.m_cxxapi_logger.m_force_flush,
            cfg.m_cxxapi_logger.m_async,
            cfg.m_cxxapi_logger.m_buffer_size,
            cfg.m_cxxapi_logger.m_strategy
        );
#endif // CXXAPI_HAS_LOGGING_IMPL

        m_inited = true;

        return true;
    }

    void c_redis::shutdown() {
#ifdef CXXAPI_HAS_LOGGING_IMPL
        m_logger.stop_async();
#endif // CXXAPI_HAS_LOGGING_IMPL

        m_inited = false;
    }
}

#endif // CXXAPI_USE_REDIS_IMPL
