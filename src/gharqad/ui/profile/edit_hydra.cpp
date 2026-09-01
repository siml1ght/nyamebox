




#include <nekobox/configs/proxy/Preset.hpp>
#include <nekobox/ui/profile/edit_hydra.h>
#include <nekobox/configs/proxy/HydraBean.hpp>

EditHydra::EditHydra(QWidget *parent) : QWidget(parent),
ui(new Ui::EditHydra) {
    ui->setupUi(this);
}

EditHydra::~EditHydra() {
    delete ui;
}

void EditHydra::onStart(std::shared_ptr<Configs::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->HydraBean();
    P_LOAD_STRING(secret_key)
    P_LOAD_STRING(hop_secret)
    ui->session_id->setText(QString::number(bean->session_id));
    ui->session_id->setValidator(QRegExpValidator_Number);
    P_LOAD_INT(hop_base)
    P_LOAD_INT(hop_range)
    P_LOAD_INT(local_relay_port)
}

bool EditHydra::onEnd() {
    auto bean = ent->unlock(ent->HydraBean());
    P_SAVE_STRING(secret_key)
    P_SAVE_STRING(hop_secret)
    bean->session_id = ui->session_id->text().toLongLong();
    P_SAVE_INT(hop_base)
    P_SAVE_INT(hop_range)
    P_SAVE_INT(local_relay_port)
    return true;
}
