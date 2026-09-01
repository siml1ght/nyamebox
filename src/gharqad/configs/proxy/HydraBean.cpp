


#include <nekobox/dataStore/ProxyEntity.hpp>
#include <nekobox/configs/proxy/AbstractBeanExtra.hpp>
#include <nekobox/configs/proxy/includes.h>

#include <qjsonobject.h>
#include <QStandardPaths>

namespace Configs {
    CoreObjOutboundBuildResult HydraBean::BuildCoreObjSingBox() const
    {
        using namespace To_CoreObj_box;
        CoreObjOutboundBuildResult result;
        QJsonObject outbound{
            {"type", "socks"},
            {"server", "127.0.0.1"},
            {"server_port", local_relay_port},
            {"version", "5"},
        };
        result.outbound = outbound;
        return result;
    }
    QString HydraBean::ToShareLink() const {
        using namespace Configs::To_Link;
        QUrl url;
        url.setScheme("hydra");
        add_default_fields(url, this);
        QUrlQuery q;
        if (!secret_key.isEmpty()) q.addQueryItem("secret_key", secret_key);
        if (!hop_secret.isEmpty()) q.addQueryItem("hop_secret", hop_secret);
        if (session_id != 0) q.addQueryItem("session_id", QString::number(session_id));
        q.addQueryItem("hop_base", QString::number(hop_base));
        q.addQueryItem("hop_range", QString::number(hop_range));
        q.addQueryItem("relay_port", QString::number(local_relay_port));
        url.setQuery(q);
        return url.toString(QUrl::FullyEncoded);
    }
    bool HydraBean::TryParseLink(const QString& link)
    {
        using namespace From_Link;
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        QUrlQuery query = GetQuery(url);
        add_default_fields(url, entity);

        if (query.hasQueryItem("secret_key")) secret_key = query.queryItemValue("secret_key");
        if (query.hasQueryItem("hop_secret")) hop_secret = query.queryItemValue("hop_secret");
        if (query.hasQueryItem("session_id")) session_id = query.queryItemValue("session_id").toLongLong();
        if (query.hasQueryItem("hop_base")) hop_base = query.queryItemValue("hop_base").toInt();
        if (query.hasQueryItem("hop_range")) hop_range = query.queryItemValue("hop_range").toInt();
        if (query.hasQueryItem("relay_port")) local_relay_port = query.queryItemValue("relay_port").toInt();
        return true;
    }
}
