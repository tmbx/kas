strings = {

    # Local pages.
    "pages.mas_config.page_subtitle" : "Teambox Main Application Server Config",
    "pages.wps_config.page_subtitle" : "Teambox Web Portal Server Config",
    "pages.tbsos_config.page_subtitle" : "TBSOS Server Config",
    "pages.freemium_config.page_subtitle" : "Teambox Freemium Config",
    "pages.basic_setup.page_subtitle" : "Basic Setup",

    # Login/logout.
    "login.bad_password" : "Bad password.",
    "locals.admin_password_not_set" : \
        "The administrator password is not currently set. Please log in the console and provide it, then try again.",

    # Basic options form for MAS.
    "forms.kcd_basic_options.legend_title" : "Basic Options",
    "forms.kcd_basic_options.mas_service.label" : "Enable service",
    "forms.kcd_basic_options.mas_service.whatsthis" : \
        "Check this to enable the Teambox Main Application Server.",
    "forms.kcd_basic_options.kcd_enforce_restriction.label" : "Enforce license restrictions",
    "forms.kcd_basic_options.kcd_enforce_restriction.whatsthis" : \
        "Check this to restrict the Teambox usage based on the license of the users.",
    "forms.kcd_basic_options.kwmo_host.label" : "WPS hostname",
    "forms.kcd_basic_options.kcd_host.label" : "MAS host",
    "forms.kcd_basic_options.kcd_host.whatsthis" : \
        "This is the hostname of the Teambox Main Application Server. The hostname must be resolvable by everyone.",
    "forms.kcd_basic_options.kwmo_host.whatsthis" : \
        "This is the hostname of the Teambox Web Portal Server. The HTML links sent in invitation e-mails will contain this" \
        + " hostname, so it must be resolvable by everyone.",
    "forms.kcd_basic_options.kcd_mail_host.label" : "SMTP server",
    "forms.kcd_basic_options.kcd_mail_host.whatsthis" : \
        "This is the hostname of your outgoing SMTP server. The MAS uses this " \
        "server to send Teambox invitation emails.",
    "forms.kcd_basic_options.kcd_mail_sender.label" : "MAS email address",
    "forms.kcd_basic_options.kcd_mail_sender.whatsthis" : \
        "This is the email address used by the MAS to send Teambox invitation "\
        "emails. Make sure this address does not trigger SPAM filtering rules.",
    "forms.kcd_basic_options.kcd_mail_auth_user.label" : "SMTP auth user",
    "forms.kcd_basic_options.kcd_mail_auth_user.whatsthis" : \
        "This is the user used to authenticate to the SMTP server.",
    "forms.kcd_basic_options.kcd_mail_auth_pwd.label" : "SMTP auth password",
    "forms.kcd_basic_options.kcd_mail_auth_pwd.whatsthis" : \
        "This is the password used to authenticate to the SMTP server.",
    "forms.kcd_basic_options.kcd_mail_auth_ssl.label" : "Secure SMTP",
    "forms.kcd_basic_options.kcd_mail_auth_ssl.whatsthis" : \
        "This checkbox must be checked if the connection to the SMTP server must "\
        "be done over SSL or TLS.",
    "forms.kcd_basic_options.kcd_kfs_mode.label" : "KFS mode",
    "forms.kcd_basic_options.kcd_kfs_mode.whatsthis" : \
        "This is the mode used when storing files. Use 'Local file system' for storing files on this server," \
        " or use 'Windows Share' to store files on another server.",
    "forms.kcd_basic_options.kcd_kfs_mode.choices.local" : "Local file system",
    "forms.kcd_basic_options.kcd_kfs_mode.choices.samba" : "Windows share",

    # Basic options form for WPS.
    "forms.kwmo_basic_options.legend_title" : "Basic Options",
    "forms.kwmo_basic_options.wps_service.label" : "Enable service",
    "forms.kwmo_basic_options.wps_service.whatsthis" : \
        "Check this to enable the Teambox Web Portal Server.",
    "forms.kwmo_basic_options.kcd_host.label" : "MAS host",
    "forms.kwmo_basic_options.kcd_host.whatsthis" : \
        "This is the hostname of the Teambox Main Application Server. The hostname must be resolvable by everyone.",
    "forms.kwmo_basic_options.kcd_pwd.label" : "MAS password",
    "forms.kwmo_basic_options.kcd_pwd.whatsthis" : "This is the password that will be used when connecting to the"\
        " Teambox Main Application Server. This defaults to the administrator password of this server.",
    "forms.kwmo_basic_options.kcd_pwd_verif.label" : "MAS password verification",
    "forms.kwmo_basic_options.kcd_pwd_verif.whatsthis" : "This is a verification of the password above.",

    # Basic options form for TBSOS.
    "forms.tbsos_basic_options.legend_title" : "Basic Options",
    "forms.tbsos_basic_options.tbsos_service.label" : "Enable service",
    "forms.tbsos_basic_options.tbsos_service.whatsthis" : \
        "Check this to enable the TBSOS service.",
    "forms.tbsos_basic_options.kcd_host.label" : "MAS host",
    "forms.tbsos_basic_options.kcd_host.whatsthis" : \
        "This is the hostname of the Teambox Main Application Server. The hostname must be resolvable by everyone.",

    # Basic options form for Freemium.
    "forms.freemium_basic_options.legend_title" : "Freemium basic options",
    "forms.freemium_basic_options.freemium_service.label" : "Enable service",
    "forms.freemium_basic_options.freemium_service.whatsthis" : \
        "Check this to enable the Freemium Service on this server.",
    "forms.freemium_basic_options.freemium_autoregister.label" : "Enable auto-register",
    "forms.freemium_basic_options.freemium_autoregister.whatsthis" : \
        "Check this to enable the users to auto-register on this server.",
    "forms.freemium_basic_options.freemium_org_id.label" : "Organization ID",
    "forms.freemium_basic_options.freemium_org_id.whatsthis" : \
        "This is the organization ID which will be used for the Freemium users. Leave empty for auto-detection.",
    "forms.freemium_basic_options.kcd_host.label" : "MAS host",
    "forms.freemium_basic_options.kcd_host.whatsthis" : \
        "This is the hostname of the Teambox Main Application Server. The hostname must be resolvable by everyone.",
    "forms.freemium_basic_options.kcd_pwd.label" : "MAS password",
    "forms.freemium_basic_options.kcd_pwd.whatsthis" : "This is the password that will be used to connect to the"\
        " Teambox Main Application Server. This defaults to the administrator password of this server.",
    "forms.freemium_basic_options.kcd_pwd_verif.label" : "MAS password verification",
    "forms.freemium_basic_options.kcd_pwd_verif.whatsthis" : "This is a verification of the password above.",

    # MAS KFS form.
    "forms.kcd_kfs_options.legend_title" : "KFS Options",
    "forms.kcd_kfs_options.kcd_smb_unc.label" : "Share UNC",
    "forms.kcd_kfs_options.kcd_smb_unc.whatsthis" : "This is the location of the Windows share used for storing" \
            " files. It must be in this form: \\\\server_x\\share_x\\ or \\\\server_x\\share_x\\dir_x\\dir_y\\ ." \
            " Forward slashes are also accepted.",
    "forms.kcd_kfs_options.kcd_smb_mount_user.label" : "Share user",
    "forms.kcd_kfs_options.kcd_smb_mount_user.whatsthis" : "This is the account name used to access the share.",
    "forms.kcd_kfs_options.kcd_smb_mount_pwd.label" : "Share password",
    "forms.kcd_kfs_options.kcd_smb_mount_pwd.whatsthis" : "This is the account password used to access the share." \
        " Using no password is supported, although using a strong password is recommended.",
    "forms.kcd_kfs_options.kcd_smb_mount_pwd_verif.label" : "Share password verification",
    "forms.kcd_kfs_options.kcd_smb_mount_pwd_verif.whatsthis" : "This is the verification of the password above.",

    # MAS organizations forms.

    # Add org.
    "forms.kcd_add_org.legend_title" : "Add organization",
    "forms.kcd_add_org.org_key_id.label" : "Key ID",
    "forms.kcd_add_org.org_key_id.whatsthis" : "This is the key ID associated to the organization you" \
        " want to allow Teambox creation in your Teambox infrastructure. You can find this ID in the" \
        " Organizations menu of your Teambox Sign-on Server administration interface.",
    "forms.kcd_add_org.org_name.label" : "Organization name",
    "forms.kcd_add_org.org_name.whatsthis" : "This is the name associated to the organization's key ID.",

    # Edit org.
    "forms.kcd_edit_org.org_key_id.label" : "Key ID",
    "forms.kcd_edit_org.org_key_id.whatsthis" : "This is the key ID associated to the organization you" \
        " want to allow Teambox creation in your Teambox infrastructure. You can find this ID in the" \
        " Organizations menu of your Teambox Sign-on Server administration interface.",
    "forms.kcd_edit_org.org_name.label" : "Organization name",
    "forms.kcd_edit_org.org_name.whatsthis" : "This is the name associated to the organization's key ID.",

    # List of organizations.
    "forms.kcd_org_list.legend_title" : "Organizations",
    "forms.kcd_org_list.doc" : "You must set here which organizations you want to be able to create Teamboxes on your" \
        " server. Usually, this will be your organization only. You can find your organization key ID in the" \
        " Organizations menu of your Teambox Sign-on Server administration interface.",
    "forms.kcd_org_list.title" : "List of organizations",
    "forms.kcd_org_list.org_key_id" : "Key ID",
    "forms.kcd_org_list.org_name" : "Organization Name",
    "forms.kcd_org_list.no_organization_yet" : "No organization yet.",

    # Validation errors.
    "validations.error.org_key_must_be_numeric" : "Organization key id must be numeric.",
    "validations.error.org_key_already_exists" : "This organization key already exists.",
    "validations.error.no_such_org_key" : "There is no such organization key.",
    "validations.error.samba_invalid_unc" : "Invalid samba UNC.",
    "validations.errors.not_positive_number" : "This must be a positive number."
}

