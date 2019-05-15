/*
 * vfio_mlx5.c
 *
 *  Created on: May 15, 2019
 *      Author: idos
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/vfio.h>
#include <linux/limits.h>


struct vfio_mlx5_device {
    int group_id;
    const char *bdf;
    void *iseg;
    uint64_t bar_size;
};


void usage(char *name)
{
    printf("usage: %s <iommu group id> <ssss:bb:dd.f>\n", name);
}


int vfio_mlx5_device_open(struct vfio_mlx5_device *dev)
{
    int ret;
    int container;
    int group;
    int device;
    int seg;
    int bus;
    int devid;
    int func;
    char path[PATH_MAX];

    struct vfio_group_status group_status = {
        .argsz = sizeof(group_status)
    };

    struct vfio_device_info device_info = {
        .argsz = sizeof(device_info)
    };

    struct vfio_region_info region_info = {
        .argsz = sizeof(region_info)
    };

    ret = sscanf(dev->bdf, "%04x:%02x:%02x.%d", &seg, &bus, &devid, &func);
    if (ret != 4) {
        printf("Failed to parse BDF\n");
        return -1;
    }

    printf("Using PCI device %04x:%02x:%02x.%d in group %d\n",
           seg, bus, devid, func, dev->group_id);

    container = open("/dev/vfio/vfio", O_RDWR);
    if (container < 0) {
        printf("Failed to open /dev/vfio/vfio, %d (%s)\n",
                container, strerror(errno));
        return -1;
    }

    ret = ioctl(container, VFIO_GET_API_VERSION);
    if (ret != VFIO_API_VERSION) {
        printf("Unknown API version\n");
        return -1;
    }

    snprintf(path, sizeof(path), "/dev/vfio/%d", dev->group_id);
    group = open(path, O_RDWR);
    if (group < 0) {
        printf("Failed to open %s, %d (%s)\n",
               path, group, strerror(errno));
        return -1;
    }

    ret = ioctl(group, VFIO_GROUP_GET_STATUS, &group_status);
    if (ret) {
        printf("ioctl(VFIO_GROUP_GET_STATUS) failed\n");
        return -1;
    }

    if (!(group_status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
        printf("Group not viable, are all devices attached to vfio?\n");
        return -1;
    }

    ret = ioctl(group, VFIO_GROUP_SET_CONTAINER, &container);
    if (ret) {
        printf("Failed to set container on group\n");
        return -1;
    }

    printf("pre-SET_CONTAINER:\n");
    printf("VFIO_CHECK_EXTENSION VFIO_TYPE1_IOMMU: %sPresent\n",
           ioctl(container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU) ?
                   "" : "Not ");
    printf("VFIO_CHECK_EXTENSION VFIO_NOIOMMU_IOMMU: %sPresent\n",
           ioctl(container, VFIO_CHECK_EXTENSION, VFIO_NOIOMMU_IOMMU) ?
                   "" : "Not ");
    printf("VFIO_CHECK_EXTENSION VFIO_TYPE1v2_IOMMU: %sPresent\n",
            ioctl(container, VFIO_CHECK_EXTENSION, VFIO_TYPE1v2_IOMMU) ?
                   "" : "Not ");

    ret = ioctl(container, VFIO_SET_IOMMU, VFIO_NOIOMMU_IOMMU);
    if (!ret) {
        printf("Incorrectly allowed no-iommu usage!\n");
        return -1;
    }

    ret = ioctl(container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);
    if (ret) {
        printf("Failed to set IOMMU\n");
        return -1;
    }

    snprintf(path, sizeof(path), "%04x:%02x:%02x.%d", seg, bus, devid, func);
    device = ioctl(group, VFIO_GROUP_GET_DEVICE_FD, path);
    if (device < 0) {
        printf("Failed to get device %s\n", path);
        return -1;
    }

    if (ioctl(device, VFIO_DEVICE_GET_INFO, &device_info)) {
        printf("Failed to get device info\n");
        return -1;
    }

    printf("Device supports %d regions, %d irqs 0x%x flags\n",
           device_info.num_regions, device_info.num_irqs, device_info.flags);

    region_info.index = 0;
    if (ioctl(device, VFIO_DEVICE_GET_REGION_INFO, &region_info)) {
        printf("Failed to get info for Region 0\n");
        return -1;

    }

    printf("size 0x%lx, offset 0x%lx, flags 0x%x\n",
           (unsigned long)region_info.size,
           (unsigned long)region_info.offset, region_info.flags);

    if (!(region_info.flags & VFIO_REGION_INFO_FLAG_MMAP)) {
        printf("Unexpected no mmap support for Region 0 in device\n");
        return -1;
    }

    void *map = mmap(NULL, (size_t)region_info.size, PROT_READ, MAP_SHARED,
                     device, (off_t)region_info.offset);

    if (map == MAP_FAILED) {
        printf("mmap failed for region 0\n");
        return -1;
    }

    dev->bar_size = (unsigned long)region_info.size;
    dev->iseg = map;
    return 0;
}


int main (int argc, char **argv)
{
    int groupid;
    struct vfio_mlx5_device dev;

    if (argc < 3) {
        usage(argv[0]);
        return -1;
    }

    if (sscanf(argv[1], "%d", &groupid) != 1) {
        usage(argv[0]);
        return -1;
    }

    memset(&dev, 0, sizeof(struct vfio_mlx5_device));
    dev.group_id = groupid;
    dev.bdf = argv[2];
    if (vfio_mlx5_device_open(&dev)) {
        printf("Error opening mlx5 device using vfio-pci\n");
        return -1;
    }

    munmap(dev.iseg, (size_t)dev.bar_size);
    return 0;
}

