import os
import subprocess
import sys
import argparse

def parse_cmd_args():

    parser = argparse.ArgumentParser(description="unpack image tool")
    parser.add_argument('-i', '--input', type=str, help='input image name', default='')
    parser.add_argument('-o', '--out', type=str, help='unpack image dir', default='out')
    parser.add_argument('-z', '--zip7z', type=str, help='7z unpack tool', default= '7z')
    parser.add_argument('-r', '--erofs', type=str, help='erofs unpack tool', default='erofsfuse')
    parser.add_argument('-m', '--mnt', type=str, help='tmp mount path', default='mnt')
    args = parser.parse_args()

    print('input dir:', args.input, '\nout dir:', args.out,
    '7z tools:', args.zip7z, '\nerofs tools:', args.erofs)
    return args

def check_files_in_directory(directory_path):
    """
    检查指定路径下是否有 *.ko 文件或同时存在 kernel 和 ramdisk 文件
    """
    ko_files = [f for f in os.listdir(directory_path) if f.endswith('.ko')]
    kernel_file = False
    ramdisk_file = False

    for root, dirs, files in os.walk(directory_path):
        for file in files:
            if file.endswith('.ko'):
                return True
            if 'kernel' in file:
                kernel_file = True
            if 'ramdisk' in file:
                ramdisk_file = True

    return kernel_file and ramdisk_file

def delete_all_in_directory(directory_path):
    """
    删除指定路径下的所有文件及目录
    """
    if not os.path.isdir(directory_path):
        print("Specified path is not a directory:", directory_path)
        return False

    for root, dirs, files in os.walk(directory_path, topdown=False):
        for name in files:
            os.remove(os.path.join(root, name))
        for name in dirs:
            os.rmdir(os.path.join(root, name))
    return True

def determine_image_format(image_path):
    """
    Determine the format of the given image file.

    :param image_path: The path to the image file
    :return: The format of the image file (e.g., 'ext2', 'ext3', 'ext4', 'erofs')
    """
    result = subprocess.run(['file', image_path], stdout=subprocess.PIPE)
    file_info = result.stdout.decode().strip()

    if 'ext2' in file_info:
        return 'ext2'
    elif 'ext3' in file_info:
        return 'ext3'
    elif 'ext4' in file_info:
        return 'ext4'
    elif 'erofs' in file_info:
        return 'erofs'
    else:
        return 'unsupported'

def unpack_image(args, image_path, output_path):
    """
    Unpack the given image (e.g., vendor_dlkm.img) to the specified output path.

    :param image_path: The path to the image file (e.g., vendor_dlkm.img)
    :param output_path: The path where the image should be unpacked
    :return: The format of the unpacked image
    """
    if not os.path.exists(image_path):
        raise FileNotFoundError("The specified image file does not exist: {}".format(image_path))

    if not os.path.exists(output_path):
        os.makedirs(output_path)

    image_format = determine_image_format(image_path)
    print("unpacked {} to: {} format {}".format(image_path,output_path,image_format))

    if image_format in ['ext2', 'ext3', 'ext4']:
        unpack_ext_image(args, image_path, output_path)
    elif image_format == 'erofs':
        unpack_erofs_image(args, image_path, output_path, args.mnt)
    else:
        print("this is {} format, try to unpack ext*".format(image_format))
        unpack_ext_image(args, image_path, output_path)
        if check_files_in_directory(output_path):
            image_format = 'ext*'
            print("Either *.ko files found or both kernel and ramdisk files are present in {} this is {}.".format(output_path,image_format))
        else:
            print("this is {} format, try unpack ext* fail, try to unpack erofs".format(image_format))
            delete_all_in_directory(output_path)
            unpack_erofs_image(args, image_path, output_path, args.mnt)
            if check_files_in_directory(output_path):
                image_format = 'erofs'
                print("Either *.ko files found or both kernel and ramdisk files are present in {} this is {}.".format(output_path,image_format))
            else:
                delete_all_in_directory(output_path)
                print("No *.ko files found and both kernel and ramdisk files are not present.")
                raise ValueError("Unsupported image format: {}".format(image_format))

    print("Image unpacked successfully to: {}".format(output_path))
    return image_format

def unpack_ext_image(args, image_path, output_path):
    """
    Unpack ext2/ext3/ext4 image using '7z' tool.

    :param image_path: The path to the ext2/ext3/ext4 image file
    :param output_path: The path where the image should be unpacked
    """
    print("ext2/ext3/ext4 image unpacked to: {}".format(output_path))

    try:
        result = subprocess.run([args.zip7z, image_path, '{}'.format(output_path)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
        print("subprocess",result.stdout.decode())
    except subprocess.CalledProcessError as e:
        print("subprocess",e.stderr.decode())

def unpack_erofs_image(args, image_path, output_path, mount_tmp_path):
    """
    Unpack erofs image using 'erofsfuse' tool.

    :param args: Arguments that include erofsfuse location
    :param image_path: The path to the erofs image file
    :param output_path: The path where the image should be unpacked
    :param mount_tmp_path: The temporary path to mount the erofs image
    """
    mount_point = os.path.join(mount_tmp_path, 'mount')
    if not os.path.exists(mount_point):
        os.makedirs(mount_point)

    mount_cmd = [args.erofs, image_path, mount_point]
    umount_cmd = ['fusermount', '-u', mount_point]

    try:
        # Mount the erofs image
        subprocess.run(mount_cmd, check=True)
        # Copy the contents from the mount point to the output path
        copy_cmd = ['cp', '-a', os.path.join(mount_point, '.'), output_path]
        subprocess.run(copy_cmd, check=True)
        print("erofs image unpacked to: {}".format(output_path))
    except subprocess.CalledProcessError as e:
        print("An error occurred:", e)
    finally:
        # Ensure the mount point is unmounted
        subprocess.run(umount_cmd, check=True)


if __name__ == '__main__':

    args = parse_cmd_args()
    try:
        image_format = unpack_image(args, args.input, args.out)
        # Return the image format to the shell
        print(image_format)
    except Exception as e:
        print("Error:", e)
        sys.exit(1)
