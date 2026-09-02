#include <cstdlib>
#include <cstring>
#include <security/pam_appl.h>
#include <unistd.h>

#include "app/user_info.h"

#include "core/log.h"

#include "modules/penance/pam_authenticator.h"

#ifndef KOKUSEI_PAM_DIR
#define KOKUSEI_PAM_DIR ""
#endif

namespace pam_auth {

namespace {

struct ConvData {
    const char *password = nullptr;
};

char *dup_cstr(const char *s) {
    if (!s)
        s = "";
    size_t n = std::strlen(s) + 1;
    char *out = static_cast<char *>(std::malloc(n));
    if (out)
        std::memcpy(out, s, n);
    return out;
}

int conversation(int num_msg, const pam_message **msg, pam_response **resp,
                 void *appdata) {
    if (num_msg <= 0 || !msg || !resp || !appdata)
        return PAM_CONV_ERR;

    auto *data = static_cast<ConvData *>(appdata);
    auto *replies = static_cast<pam_response *>(
        std::calloc(static_cast<size_t>(num_msg), sizeof(pam_response)));
    if (!replies)
        return PAM_BUF_ERR;

    for (int i = 0; i < num_msg; ++i) {
        if (!msg[i]) {
            std::free(replies);
            return PAM_CONV_ERR;
        }
        switch (msg[i]->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
            replies[i].resp = dup_cstr(data->password);
            break;
        case PAM_PROMPT_ECHO_ON:
            replies[i].resp = dup_cstr("");
            break;
        default:
            replies[i].resp = nullptr;
            break;
        }
    }
    *resp = replies;
    return PAM_SUCCESS;
}

bool pam_config_present() {
    return KOKUSEI_PAM_DIR[0] != '\0' &&
           ::access(KOKUSEI_PAM_DIR "/kokusei", R_OK) == 0;
}

} // namespace

void secure_clear(std::string &value) {
    volatile char *p = value.empty() ? nullptr : value.data();
    for (size_t i = 0; i < value.size(); ++i)
        p[i] = '\0';
    value.clear();
}

Result authenticate_current_user(std::string_view password) {
    std::string user = user_info::username();
    if (user.empty() || user == "unknown")
        return {false, "could not determine current user"};

    std::string pw(password);
    ConvData conv_data{pw.c_str()};
    pam_conv conv{&conversation, &conv_data};

    pam_handle_t *pamh = nullptr;
    int rc;
    if (pam_config_present())
        rc = pam_start_confdir("kokusei", user.c_str(), &conv, KOKUSEI_PAM_DIR,
                               &pamh);
    else
        rc = pam_start("login", user.c_str(), &conv, &pamh);

    if (rc != PAM_SUCCESS || !pamh) {
        klog("penance: pam_start failed rc=%d", rc);
        secure_clear(pw);
        return {false, "authentication unavailable"};
    }

    rc = pam_authenticate(pamh, 0);
    if (rc == PAM_SUCCESS) {
        int acct = pam_acct_mgmt(pamh, 0);
        if (acct != PAM_SUCCESS && acct != PAM_AUTHINFO_UNAVAIL)
            rc = acct;
    }

    std::string msg;
    if (rc != PAM_SUCCESS) {
        const char *err = pam_strerror(pamh, rc);
        msg = err ? err : "authentication failed";
    }

    pam_end(pamh, rc);
    secure_clear(pw);

    if (rc == PAM_SUCCESS)
        return {true, {}};
    return {false, msg};
}

} // namespace pam_auth
