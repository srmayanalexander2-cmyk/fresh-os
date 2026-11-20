EDITIONS = ("Normal", "Lite", "VMware", "VMware-Lite")


def lite(edition):
    return edition in ("Lite", "VMware-Lite")


def vmware(edition):
    return edition in ("VMware", "VMware-Lite")
