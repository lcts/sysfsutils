/*
 * sysfs_device.c
 *
 * Generic device utility functions for libsysfs
 *
 * Copyright (C) IBM Corp. 2003
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#include "libsysfs.h"
#include "sysfs.h"

/**
 * sysfs_del_device: routine for dlist integration
 */
static void sysfs_del_device(void *dev)
{
	sysfs_close_device((struct sysfs_device *)dev);
}

/**
 * sysfs_close_device: closes and cleans up a device
 * @dev = device to clean up
 */
void sysfs_close_device(struct sysfs_device *dev)
{
	if (dev != NULL) {
		if (dev->directory != NULL)
			sysfs_close_directory(dev->directory);
		if (dev->children != NULL && dev->children->count == 0)
			dlist_destroy(dev->children);
		free(dev);
	}
}

/**
 * alloc_device: allocates and initializes device structure
 * returns struct sysfs_device
 */
static struct sysfs_device *alloc_device(void)
{
	return (struct sysfs_device *)calloc(1, sizeof(struct sysfs_device));
}

/**
 * sysfs_get_device_attr: searches dev's attributes by name
 * @dev: device to look through
 * @name: attribute name to get
 * returns sysfs_attribute reference with success or NULL with error.
 */
struct sysfs_attribute *sysfs_get_device_attr(struct sysfs_device *dev,
						const unsigned char *name)
{
	struct sysfs_attribute *cur = NULL;
	unsigned char attrname[SYSFS_NAME_LEN];

	if (dev == NULL || dev->directory == NULL 
	    || dev->directory->attributes == NULL || name == NULL) {
		errno = EINVAL;
		return NULL;
	}
	dlist_for_each_data(dev->directory->attributes, cur,
			struct sysfs_attribute) {
		if ((sysfs_get_name_from_path(cur->path, attrname, 
		    SYSFS_NAME_LEN)) != 0) 
			continue;
		if (strcmp(name, attrname) != 0)
			continue;

		return cur;
	}

	return NULL;
}

/**
 * sysfs_open_device: opens and populates device structure
 * @path: path to device, this is the /sys/devices/ path
 * returns sysfs_device structure with success or NULL with error
 */
struct sysfs_device *sysfs_open_device(const unsigned char *path)
{
	struct sysfs_device *dev = NULL;
	struct sysfs_directory *sdir = NULL;
	unsigned char *p = NULL;

	if (path == NULL) {
		errno = EINVAL;
		return NULL;
	}
	dev = alloc_device();	
	if (dev == NULL) {
		dprintf("Error allocating device at %s\n", path);
		return NULL;
	}
	sdir = sysfs_open_directory(path);
	if (sdir == NULL) {
		dprintf("Invalid device at %s\n", path);
		errno = EINVAL;
		sysfs_close_device(dev);
		return NULL;
	}
	if ((sysfs_read_directory(sdir)) != 0) {
		dprintf("Error reading device directory at %s\n", path);
		sysfs_close_directory(sdir);
		sysfs_close_device(dev);
		return NULL;
	}
	dev->directory = sdir;
	strcpy(dev->bus_id, sdir->name);
	sysfs_find_device_bus_name(dev->bus_id, dev->bus_name, SYSFS_NAME_LEN);

	/* get device name */
	p = sysfs_get_value_from_attributes(sdir->attributes,	
							SYSFS_NAME_ATTRIBUTE);
	if (p != NULL) {
		strncpy(dev->name, p, SYSFS_NAME_LEN);
		p = dev->name + strlen(dev->name) - 1;
		if ((strlen(dev->name) > 0) && *p == '\n')
			*p = '\0';
	}

	return dev;
}

/**
 * sysfs_close_device_tree: closes every device in the supplied tree, 
 * 	closing children only.
 * @devroot: device root of tree.
 */
void sysfs_close_device_tree(struct sysfs_device *devroot)
{
	if (devroot != NULL) {
		if (devroot->children != NULL) {
			struct sysfs_device *child = NULL;

			dlist_for_each_data(devroot->children, child,
					struct sysfs_device) {
				sysfs_close_device_tree(child);
			}
		}
		sysfs_close_device(devroot);
	}
}

/**
 * sysfs_open_device_tree: opens root device and all of its children,
 *	creating a tree of devices. Only opens children.
 * @path: sysfs path to devices
 * returns struct sysfs_device and its children with success or NULL with
 *	error.
 */
struct sysfs_device *sysfs_open_device_tree(const unsigned char *path)
{
	struct sysfs_device *rootdev = NULL, *new = NULL;
	struct sysfs_directory *cur = NULL;

	if (path == NULL) {
		errno = EINVAL;
		return NULL;
	}
	rootdev = sysfs_open_device(path);
	if (rootdev == NULL) {
		dprintf("Error opening root device at %s\n", path);
		return NULL;
	}
	if (rootdev->directory->subdirs != NULL) {
		dlist_for_each_data(rootdev->directory->subdirs, cur,
				struct sysfs_directory) {
			new = sysfs_open_device_tree(cur->path);
			if (new == NULL) {
				dprintf("Error opening device tree at %s\n",
					cur->path);
				sysfs_close_device_tree(rootdev);
				return NULL;
			}
			if (rootdev->children == NULL)
				rootdev->children = dlist_new_with_delete
					(sizeof(struct sysfs_device),
					sysfs_del_device);
			dlist_unshift(rootdev->children, new);
		}
	}

	return rootdev;
}
