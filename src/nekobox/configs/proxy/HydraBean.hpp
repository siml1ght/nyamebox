#pragma once

#include "AbstractBean.hpp"

namespace Configs {
    class HydraBean : public AbstractBean {
    public:
        QString secret_key = "";
        QString hop_secret = "";
        long long session_id = 0;
        int hop_base = 44000;
        int hop_range = 10;
        int local_relay_port = 16335;

        HydraBean(Configs::ProxyEntity * entity) : AbstractBean(entity, 0) {}

        INIT_BEAN_MAP
            ADD_MAP("secret_key", secret_key, string);
            ADD_MAP("hop_secret", hop_secret, string);
            ADD_MAP("session_id", session_id, longlong);
            ADD_MAP("hop_base", hop_base, int);
            ADD_MAP("hop_range", hop_range, int);
            ADD_MAP("local_relay_port", local_relay_port, int);
        STOP_MAP

        CoreObjOutboundBuildResult BuildCoreObjSingBox() const override;

        bool TryParseLink(const QString &link) override;

        QString ToShareLink() const override;

        virtual QString type()const override {
            return "hydra";
        };
    };
} // namespace Configs
