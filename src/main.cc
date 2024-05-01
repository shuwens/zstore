#include <fmt/core.h>
#include <libxnvme.h>

#include "utils.hpp"

int main(int argc, char **argv)
{
    auto dev_name = "/dev/nvme0";

    xnvme_opts opts = xnvme_opts_default();
    xnvme_dev *dev = xnvme_dev_open(dev_name, &opts);
    check_cond(dev == nullptr, "Failed to open device {}", dev_name);

    xnvme_dev_pr(dev, XNVME_PR_DEF);
    xnvme_dev_close(dev);

    return 0;
}