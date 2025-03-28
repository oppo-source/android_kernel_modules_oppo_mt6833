import os
import requests
import subprocess
import gzip
import shutil
from datetime import datetime
import csv
import argparse
import re
import sys
import fnmatch
import hashlib
import json
from requests.auth import HTTPBasicAuth
import datetime

file_paths = [
    "boot.img",
    "boot-lz4.img",
    "boot-gz.img",
    "BUILD_INFO.txt",
    "system_dlkm.img",
    "vmlinux",
    "md5_sums.txt",
    "gki-info.txt",
    "vmlinux.symvers"
]
remote_base_address = "xxx/aosp-gki-image-local/"
remote_api_address  = "xxx/aosp-gki-image-local/"

# Function to check validity of date
def is_valid_date(date):
    year, month = map(int, date.split('-'))
    return 1 <= month <= 12

def validate_gki_file_name(file_path):
    # Get the file name
    dir_name = os.path.basename(os.path.normpath(file_path))

    # Use regular expression to extract the specified fields
    regex = r"(\w+)-(\d+\.\d+)-(\d{4}-\d{2})_r(\d+)"
    matches = re.match(regex, dir_name)

    parts = dir_name.split('-')
    # 获取第一个字段 android15-6.6
    android_version = '-'.join(parts[:2])

    # 获取第二个字段 android15-6.6-2024-06
    tmp_release_date = '-'.join(parts[:4])
    android_version_release_date = tmp_release_date.split('_')[0]
    #print("version: {} \nversion_date: {}".format(android_version, android_version_release_date))
    if matches:
        # 获取结果
        version = matches.group(1)
        kernel_version = matches.group(2)
        date = matches.group(3)
        release = matches.group(4)

        # 进行格式和有效性判断
        if version.startswith('android') and version[7:].isdigit() and \
            re.match(r'\d+\.\d+', kernel_version) and \
            is_valid_date(date) and release.isdigit():
            #print("Android version:", version)
            #print("Kernel version:", kernel_version)
            #print("Date: ", date)
            #print("Release: ", release)
            return android_version, android_version_release_date, dir_name
        else:
            print("File path format error,you need input file name {} like android15-6.6-2024-06_r1".format(dir_name))
            return None, None, None
    else:
        print("No matching file path found,you need input file name {} like android15-6.6-2024-06_r1".format(dir_name))
        return None, None, None

def validate_ogki_file_name(file_path):
    # Get the file name
    dir_name = os.path.basename(os.path.normpath(file_path))

    # Use regular expression to extract the specified fields
    regex = r"(\w+)-(\d+\.\d+)-(\d{4}-\d{2})-(\w+)_r(\d+)"
    matches = re.match(regex, dir_name)

    parts = dir_name.split('-')
    # 获取第一个字段 android15-6.6
    android_version = '-'.join(parts[:2])

    # 获取第二个字段 android15-6.6-2024-06
    tmp_release_date = '-'.join(parts[:4])
    android_version_release_date = tmp_release_date.split('_')[0]
    #print("version: {} \nversion_date: {}".format(android_version, android_version_release_date))
    if matches:
        # 获取结果
        version = matches.group(1)
        kernel_version = matches.group(2)
        date = matches.group(3)
        ogki = matches.group(4)
        release = matches.group(5)

        # 进行格式和有效性判断
        if version.startswith('android') and version[7:].isdigit() and \
            re.match(r'\d+\.\d+', kernel_version) and \
            ogki == "ogki" and \
            is_valid_date(date) and release.isdigit():
            #print("Android version:", version)
            #print("Kernel version:", kernel_version)
            #print("Date:", date)
            #print("ogki:", ogki)
            #print("Release:", release)
            return android_version, android_version_release_date, dir_name
        else:
            print("File path format error,you need input file name {} like android15-6.6-2024-06-ogki_r1".format(dir_name))
            return None, None, None
    else:
        print("No matching file path found,you need input file name {} like android15-6.6-2024-06-ogki_r1".format(dir_name))
        return None, None, None

def read_content(file_path, key):
    try:
        with open(file_path, 'r') as file:
            for line in file:
                # 去除行首尾的空白字符，并检查是否以特定键开头
                if line.strip().startswith(key):
                    return line.strip()
    except FileNotFoundError:
        print("error! config file not found in ",file_path)
        sys.exit(1)

def write_content(file_path, key, new_content):
    found = False
    lines = []

    # 检查文件是否存在
    print("write {} to {}\n".format(new_content, file_path))
    if os.path.exists(file_path):
        with open(file_path, 'r') as file:
            lines = file.readlines()
    new_lines = []
    # 更新内容
    for line in lines:
        if line.strip().startswith(key):
            new_lines.append(new_content + '\n')
            found = True
        else:
            new_lines.append(line)

    # 如果没有找到对应的行，则添加
    if not found:
        new_lines.append(new_content + '\n')

    with open(file_path, 'w') as file:
        file.writelines(new_lines)

def get_resource_path(relative_path):
    """获取资源文件的路径，兼容打包和未打包两种情况"""
    if hasattr(sys, '_MEIPASS'):
        # 打包后的临时目录路径
        base_path = sys._MEIPASS
    else:
        # 未打包时的脚本目录路径
        base_path = os.path.dirname(os.path.realpath(__file__))
    return os.path.join(base_path, relative_path)

def parse_cmd_args():

    parser = argparse.ArgumentParser(description="ogki gki image download upload check")
    parser.add_argument('-t', '--transfer', type=str, help='download or upload image', default='download')
    parser.add_argument('-i', '--input', type=str, help='upload image save dir', default='')
    parser.add_argument('-u', '--user', type=str, help='upload user name', default='')
    parser.add_argument('-p', '--password', type=str, help='upload password', default='')
    parser.add_argument('-o', '--out', type=str, help='download image save dir', default='out')
    parser.add_argument('-k', '--kernel', type=str, help='GKI/OGKI type', default='GKI')
    parser.add_argument('-b', '--boot', type=str, help='boot.img type boot-gz.img boot.img boot-lz4.img', default='')
    parser.add_argument('-a', '--address', type=str, help='image server address', default=remote_base_address)
    parser.add_argument('-z', '--lz4', type=str, help='lz4 process tool', default= get_resource_path('lz4'))
    parser.add_argument('-n', '--unpack_bootimg', type=str, help='boot unpack tool', default=get_resource_path('unpack_bootimg'))
    parser.add_argument('-m', '--md5_sums', type=str, help='boot unpack tool', default='md5_sums.txt')
    parser.add_argument('-c', '--config', type=str, help='download config file', default='config.txt')
    parser.add_argument('-f', '--force', action='store_true', help='force update', default=False)
    args = parser.parse_args()

    if args.kernel.islower():
        args.kernel = args.kernel.upper()
    if args.kernel not in ["GKI", "OGKI"]:
        args.kernel = "GKI"

    if args.transfer == "upload" or args.transfer == "get":
        if args.kernel == "GKI":
            android_version, android_version_release_date, dir_name \
            = validate_gki_file_name(args.input)
        else:
            android_version, android_version_release_date, dir_name \
            = validate_ogki_file_name(args.input)
        if android_version is None or android_version_release_date is None \
                or dir_name is None:
            print("Invalid input string format.")
            return False

        relative_path = "{}/{}/{}/{}".format(
                        args.kernel.rstrip('/'),
                        android_version.strip('/'),
                        android_version_release_date.strip('/'),
                        dir_name.strip('/')
                    )

        if args.transfer == "upload":
            write_content("{}/{}".format(args.out.rstrip('/'), args.config.lstrip('/')), args.kernel, relative_path)
        else:
            args.address =  remote_api_address
    elif args.transfer == "dump":
        relative_path = ""
        pass
    else:
        relative_path = read_content("{}/{}".format(args.out.rstrip('/'), args.config.lstrip('/')), args.kernel)
        print("in {}\nCurrent TAG is {}".format(args.out + '/' + args.config, relative_path))
        if not relative_path:
            print("relative_path is {}".format(relative_path))
            return False

        if args.kernel == "GKI":
            android_version, android_version_release_date, dir_name \
            = validate_gki_file_name(relative_path)
        else:
            android_version, android_version_release_date, dir_name \
            = validate_ogki_file_name(relative_path)
        if android_version is None or android_version_release_date is None \
                or dir_name is None:
            print("Invalid path from config")
            return False

    args.address =  args.address + relative_path
    print('transfer type:', args.transfer, '\ninput dir:', args.input,'\nout dir:', args.out,
      '\nkernel ogki/gki:', args.kernel, '\nboot:', args.boot, '\nurl:', args.address,
      '\nlz4:', args.lz4, '\nunpack_bootimg:', args.unpack_bootimg,
      '\nmd5_sums:', args.md5_sums, '\nconfig:', args.config, '\nforce:', args.force)
    return args

def upload_files_to_server(user, password, local_dir, file_paths, remote_base_address):
    for file_name in file_paths:
        local_path = os.path.join(local_dir, file_name)
        remote_path = os.path.join(remote_base_address, file_name)
        if not os.path.exists(local_path):
            print("Error: {} not exist please check again\n".format(local_path))
            sys.exit(1)
        curl_command = [
            "curl",
            "-u", "{}:{}".format(user, password),
            "-T", local_path,
            remote_path
        ]

        try:
            result = subprocess.run(curl_command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)

            # Check if stdout contains a JSON error message
            try:
                stdout_json = json.loads(result.stdout)
                if 'errors' in stdout_json:
                    print("Failed to upload {} to {}".format(local_path, remote_path))
                    for error in stdout_json['errors']:
                        print("Error: Status {} - Message: {}".format(error.get('status', 'Unknown'), error.get('message', 'No message')))
                        if "Unauthorized" in error.get('message', 'No message'):
                            print("please input user name and password")
                    continue
            except json.JSONDecodeError:
                pass  # stdout is not JSON, might be normal output

            if result.returncode != 0:
                print("Failed to upload {} to {}".format(local_path, remote_path))
                print("Error: {}".format(result.stderr))
            else:
                print("Successfully uploaded {} to {}".format(local_path, remote_path))
        except Exception as e:
            print("Exception occurred while uploading {} to {}: {}".format(local_path, remote_path, e))

def remove_local_files(local_dir, file_paths):
    for file_name in file_paths:
        local_path = os.path.join(local_dir, file_name)
        if os.path.exists(local_path):
            os.remove(local_path)

def download_files_from_server(local_dir, file_paths, boot, remote_base_address):
    if not os.path.exists(local_dir):
        os.makedirs(local_dir)

    for file_name in file_paths:
        if boot and fnmatch.fnmatch(file_name, "boot*.img"):
            if file_name == boot:
                local_path = os.path.join(local_dir, "boot.img")
                print("download {} to boot.img".format(file_name))
            else:
                print("current type is {} ignore {}".format(boot, file_name))
                continue
        else:
            local_path = os.path.join(local_dir, file_name)

        remote_path = os.path.join(remote_base_address, file_name)

        curl_command = [
            "curl",
            "-f",  # Fail silently (no output at all) on server errors
            "-s",  # Silent mode
            "--write-out", "%{http_code}",  # Write HTTP status code to output
            "-o", local_path,
            remote_path
        ]

        #print("Downloading {} to {}...".format(remote_path, local_path))
        try:
            result = subprocess.run(curl_command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
            output = result.stdout.strip()
            http_code = output[-3:]  # Get the last 3 characters which represent the HTTP status code
            if result.returncode == 0 and http_code == "200":
                print("Successfully downloaded {} to {}".format(remote_path, local_path))
            else:
                print("output: {} result{}".format(output, result))
                print("Failed to download {} from {}".format(file_name, remote_path))
                print("Error: stderr {} stdout {} returncode {}".format(result.stderr, result.stdout, result.returncode))
            if not os.path.exists(local_path):
                print("Error: {} not exist please check again\n".format(local_path))
                remove_local_files(local_dir, file_paths)
                sys.exit(1)

        except Exception as e:
            print("Exception occurred while downloading {} from {}: {}".format(file_name, remote_path, e))

def unpack_boot_img(unpack_bootimg_path, boot_img_path, output_dir):
    if not os.path.exists(boot_img_path):
        print("{} File does not exist,return".format(boot_img_path))
        sys.exit(1)
        return False
    args = ["--boot_img", boot_img_path, "--out", output_dir, "--format=mkbootimg"]
    process = subprocess.Popen([unpack_bootimg_path] + args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()

    if process.returncode != 0:
        print("Error executing command: ", stderr.decode())
        return False
    else:
        print("Command executed successfully: ", stdout.decode())
        return True

def process_lz4_kernel(lz4_tool_path, kernel_path):
    print("{}".format(kernel_path))
    lz4_kernel_path = "{}.lz4".format(kernel_path)
    print("{}".format(lz4_kernel_path))
    os.rename(kernel_path, lz4_kernel_path)
    with open(kernel_path, 'w') as f:
        subprocess.run([lz4_tool_path, '-d', '-c', lz4_kernel_path], stdout=f)

def decompress_gz_file(input_file_path, output_file_path):
    gz_file_path = '{}.gz'.format(input_file_path)
    os.rename(input_file_path, gz_file_path)
    with gzip.open(gz_file_path, 'rb') as f_in:
        with open(output_file_path, 'wb') as f_out:
            shutil.copyfileobj(f_in, f_out)
    os.remove(gz_file_path)

def get_compression_type(file_path,lz4):
    if not os.path.exists(file_path):
        print("{} File does not exist.ignore".format(file_path))
        return 'normal'
    command = ["file", file_path]
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)

    if "LZ4 compressed data" in result.stdout:
        process_lz4_kernel(lz4, file_path)
        return 'lz4'
    elif "gzip compressed data" in result.stdout:
        decompress_gz_file(file_path, file_path)
        return 'gz'
    else:
        return 'normal'

def find_linux_versions(file_path):
    if not os.path.exists(file_path):
        print("{} File does not exist,ignore".format(file_path))
        return ""
    command = "strings {} | grep 'Linux version.*SMP PREEMPT.*'".format(file_path)
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, shell=True)

    lines = result.stdout.strip().split('\n')
    print(lines)
    if not lines or lines == ['']:
        print("Cannot find version information")
        raise ValueError("Cannot find version information")

    versions = [line.split()[2] for line in lines]
    print(versions)
    if len(versions) > 1 and len(set(versions)) > 1:
        print("Version information is inconsistent")
        raise ValueError("Version information is inconsistent")
    elif versions:
        return versions[0]
    else:
        print("Cannot find version information")
        raise ValueError("Cannot find version information")

def is_valid_string(input_string, validation_type):
    print("input is {} needed is {}".format(input_string, validation_type))
    if validation_type == "OGKI":
        # 检查字符串是否包含 -ab_o_
        if "-ab_o_" in input_string:
            print("type is {} -ab_o_ in string {}".format(validation_type, input_string))
            return True
        else:
            print("type is {} -ab_o_ not in string {}".format(validation_type, input_string))
            return False
    elif validation_type == "GKI":
        # 检查字符串是否包含 -ab 但不包含 -ab_o_
        if "-ab" in input_string and "-ab_o_" not in input_string:
            print("type is {} only -ab in string {}".format(validation_type, input_string))
            return True
        else:
            print("type is {} except only -ab(not -ab_o_) in string {}".format(validation_type, input_string))
            print("you need upload a GKI image come from Google")
            return False
    else:
        # 其他验证类型，返回 False
        return False

def calculate_md5sum(file_paths, output_file, base_path="."):
    """
    计算指定路径下文件的MD5sum值，并将结果输出到指定的文件中

    :param file_paths: 文件名列表
    :param output_file: 输出文件路径
    :param base_path: 文件目录路径（默认当前目录）
    """

    def get_md5(file_path):
        """计算指定文件的MD5值"""
        hash_md5 = hashlib.md5()
        try:
            with open(file_path, "rb") as f:
                for chunk in iter(lambda: f.read(4096), b""):
                    hash_md5.update(chunk)
            return hash_md5.hexdigest()
        except FileNotFoundError:
            print("File not found: {}".format(file_path))
            return None
        except Exception as e:
            print("Error reading file {}: {}".format(file_path, e))
            return None

    with open(output_file, "w") as out_f:
        for file_name in file_paths:
            file_path = os.path.join(base_path, file_name)
            md5sum = get_md5(file_path)
            if md5sum:
                out_f.write("{}: {}\n".format(file_name, md5sum))
                #print(f"{file_name}: {md5sum}")
            else:
                out_f.write("{}: ERROR\n".format(file_name))
                print("{}: ERROR".format(file_name))

def delete_files_and_dirs(path):
    # Ensure the provided directory exists
    if not os.path.exists(path):
        print('The provided directory does not exist.')
        return

    if os.path.isdir(path):
        shutil.rmtree(path)

def check_image_is_valid(args, bootimg, unpack, kernel):

    print("\nuppack boot.img for check boot is valid or not")
    unpack_boot_img(args.unpack_bootimg, bootimg, unpack)
    print("get kernel compression type and decompress")
    compression_type = get_compression_type(kernel,args.lz4)
    print("current compression type ",compression_type)
    try:
        print("get kernel linux kernel version")
        version = find_linux_versions(kernel)
        print("\nlinux_kernel_version",version)
    except ValueError as e:
        print(str(e))

    image_valid = is_valid_string(version,args.kernel)
    print("check image valid is",image_valid)
    delete_files_and_dirs(unpack)
    return image_valid

def check_image_need_upload(address):
    try:
        #print("address.",address)
        response_base = requests.get(remote_base_address, stream=True)
        response_current = requests.get(address, stream=True)
    except ImportError:
        print("SSL module is not available. Can't connect to HTTPS URL.")
        return False
    except Exception as e:
        print('An unexpected error occurred:', str(e))
        return False
    print("base status code {} current status code {}".format(response_base.status_code,response_current.status_code))
    if response_base.status_code == 200 and response_current.status_code != 200:
        print("connect is ok and image not exist,need upload\n")
        return True
    return False

def check_image_need_download(address,file_name):
    try:
        print("url {} file_name {}".format(address, file_name))
        response_current = requests.get(address, stream=True)
    except ImportError:
        print("SSL module is not available. Can't connect to HTTPS URL.")
        return False
    except Exception as e:
        print('An unexpected error occurred:', str(e))
        return False

    print("current status code {} \nfile exist status {}".format(response_current.status_code,os.path.exists(file_name)))
    if response_current.status_code == 200 and not os.path.exists(file_name):
        print("connect is ok and image not exist,need download\n")
        return True
    elif response_current.status_code != 200:
        print("connect can't access,please check {} if exist".format(address))
    else:
        print("check if {} exist".format(file_name))
    return False

def get_parent_url(url):
    if url.endswith('/'):
        url = url[:-1]
    return url.rsplit('/', 1)[0]

def natural_sort_key(element):
    return [int(s) if s.isdigit() else s for s in re.split(r'(\d+)', element)]

def get_artifactory_directories(url, config, api_key=None):
    # 获取父级URL
    parent_url = get_parent_url(url)
    print('url {}\nparent_url {} '.format(url, parent_url))

    headers = {}
    if api_key:
        headers = {"X-JFrog-Art-Api": api_key}

    response = requests.get(parent_url+'/', headers=headers)

    if response.status_code == 200:
        data = json.loads(response.text)
        directories = [child["uri"].strip("/") for child in data["children"] if child["folder"]]

        # 按照自然排序规则对目录进行排序
        directories.sort(key=natural_sort_key)

        # 写入config文件
        with open(config, 'w') as f:
            for directory in directories:
                f.write('{}\n'.format(directory))

        # 单独打印并注明最后的一个排序目录
        print('tag will write to {}'.format(config))
        print('last tag is : {}'.format(directories[-1]))
        return directories
    else:
        raise Exception("请求失败，状态码：{}".format(response.status_code))

def get_time_difference_from_last_updated(last_updated):
    try:
        # Convert last_updated to datetime object
        last_updated_date = datetime.datetime.strptime(last_updated, '%Y-%m-%dT%H:%M:%S.%f%z')
        # Get current time in UTC
        now = datetime.datetime.utcnow().replace(tzinfo=datetime.timezone.utc)
        # Calculate time difference
        time_diff = now - last_updated_date
        # Return the number of days
        return max(time_diff.days, 0)  # Ensure non-negative days
    except Exception as e:
        print("Error parsing last_updated time: {}".format(e))
        return 'N/A'

def get_file_details(url_details, url_stats, username=None, password=None):
    auth = None
    if username and password:
        auth = (username, password)

    # Get file basic details
    response_details = requests.get(url_details, auth=auth)
    if response_details.status_code != 200:
        print("Failed to get details for {}".format(url_details))
        print("HTTP Status Code: {}".format(response_details.status_code))
        print("Response: {}".format(response_details.text))
        return None

    file_info = response_details.json()

    # Get file stats
    response_stats = requests.get(url_stats, auth=auth)
    if response_stats.status_code != 200:
        print("Failed to get stats for {}".format(url_stats))
        print("HTTP Status Code: {}".format(response_stats.status_code))
        print("Response: {}".format(response_stats.text))
        return None

    file_stats = response_stats.json()

    # Parse and collect relevant information
    details = {
        'path': file_info.get('path', 'N/A'),
        'last_updated': file_info.get('lastUpdated', 'N/A'),
        'download_uri': file_info.get('downloadUri', 'N/A'),
        'download_count': file_stats.get('downloadCount', 'N/A'),
    }

    # Convert lastDownloaded to readable format and calculate time difference
    last_downloaded_timestamp = file_stats.get('lastDownloaded', 0)
    details['last_downloaded'] = last_downloaded_timestamp

    if last_downloaded_timestamp != 0:
        last_downloaded_date = datetime.datetime.fromtimestamp(last_downloaded_timestamp / 1000.0)
        details['last_downloaded_readable'] = last_downloaded_date.strftime('%Y-%m-%d %H:%M:%S')
        now = datetime.datetime.utcnow()
        time_diff = now - last_downloaded_date
        details['days_since_last_download'] = max(time_diff.days, 0)  # Ensure non-negative days

    else:
        details['last_downloaded_readable'] = 0
        details['days_since_last_download'] = get_time_difference_from_last_updated(details['last_updated'])

    details['url_details'] = url_details
    details['url_stats'] = url_stats

    return details

def list_directory_and_collect_details(base_url, repo_path, username=None, password=None):
    detailed_file_infos = []

    def list_directory(path):
        url = "{}/{}".format(base_url.rstrip('/'), path.lstrip('/'))
        auth = None
        if username and password:
            auth = (username, password)

        response = requests.get(url, auth=auth)
        if response.status_code == 200:
            data = response.json()
            if 'children' in data:
                for child in data['children']:
                    child_path = "{}/{}".format(path, child['uri'].lstrip('/'))
                    if child.get('folder', False):
                        list_directory(child_path)
                    else:
                        url_details = "{}/{}".format(base_url.rstrip('/'), child_path.lstrip('/'))
                        url_stats = "{}?stats".format(url_details)
                        file_details = get_file_details(url_details, url_stats, username, password)
                        if file_details:
                            detailed_file_infos.append(file_details)
        else:
            print("Failed to list directory {}".format(path))
            print("HTTP Status Code: {}".format(response.status_code))
            print("Response: {}".format(response.text))

    list_directory(repo_path)
    return detailed_file_infos

def output_details_to_file(detailed_file_infos, output_file):
    if output_file.lower().endswith('.json'):
        with open(output_file, 'w') as f:
            json.dump(detailed_file_infos, f, indent=4)
        print("Detailed file information saved to {}".format(output_file))
    else:  # scv type
        # 获取目录路径和文件名
        file_dir = os.path.dirname(output_file)
        file_name = os.path.basename(output_file)

        # 改变文件名的后缀为 .csv
        file_name = os.path.splitext(file_name)[0] + ".csv"

        # 合并目录路径和新的文件名
        new_output_file = os.path.join(file_dir, file_name)

        keys = detailed_file_infos[0].keys() if detailed_file_infos else ['path', 'last_updated', 'download_uri', 'download_count', 'last_downloaded', 'last_downloaded_readable', 'days_since_last_download']
        with open(new_output_file, 'w', newline='') as f:
            dict_writer = csv.DictWriter(f, fieldnames=keys)
            dict_writer.writeheader()
            dict_writer.writerows(detailed_file_infos)
        print("Detailed file information saved to {}".format(new_output_file))

def dump_file_details(args):
    repo_path = ""  # Root path, change if you start from a subdirectory
    output_file = "{}/{}".format(args.out.rstrip('/'), args.config.lstrip('/'))

    detailed_file_infos = list_directory_and_collect_details(remote_api_address, repo_path)
    output_details_to_file(detailed_file_infos, output_file)

    # Print details of files not downloaded for more than a year
    one_year_days = 365
    for info in detailed_file_infos:
        if info['days_since_last_download'] != 'N/A' and info['days_since_last_download'] > one_year_days:
            print("File: {} has not been downloaded for more than a year.".format(info['path']))
            print("Details: Last downloaded: {}, Download URI: {}, Download Count: {}".format(info['last_downloaded_readable'], info['download_uri'], info['download_count']))
            print()

def main():
    print('\nget input argc/argv')
    print('\nyou can read this link for more help\n\n')

    args = parse_cmd_args()

    if args == False:
        sys.exit(1)
    if args.transfer == "upload":
        bootimg = os.path.join(args.input, "boot.img")
        unpack = os.path.join(args.input, "unpack")
        kernel = os.path.join(unpack, "kernel")
        image_valid = check_image_is_valid(args, bootimg, unpack, kernel)
        need_upload = check_image_need_upload("{}/{}".format(args.address.rstrip('/'), args.md5_sums.lstrip('/')))
        if args.force or (image_valid == True and need_upload == True):
            calculate_md5sum(file_paths, "{}/{}".format(args.input.rstrip('/'), args.md5_sums.lstrip('/')), base_path=args.input)
            upload_files_to_server(args.user, args.password, args.input, file_paths, args.address)
        else:
            if image_valid == False:
               print("image is invalid,Please check again")
               sys.exit(1)
            if need_upload == False:
               print("{} is exist,default ignore".format(args.address +'/'+ args.md5_sums))
    elif args.transfer == "get":
        get_artifactory_directories(args.address, "{}/{}".format(args.input.rstrip('/'), args.config.lstrip('/')))
    elif args.transfer == "dump":
        dump_file_details(args)
    else:
        bootimg = os.path.join(args.out, "boot.img")
        unpack = os.path.join(args.out, "unpack")
        kernel = os.path.join(unpack, "kernel")
        need_download = check_image_need_download(
             "{}/{}".format(args.address.rstrip('/'), args.md5_sums.lstrip('/')),
             "{}/{}".format(args.out.rstrip('/'), args.md5_sums.lstrip('/')))
        if args.force or (need_download == True):
            download_files_from_server(args.out, file_paths, args.boot, args.address)
            image_valid = check_image_is_valid(args, bootimg, unpack, kernel)
            if image_valid != True:
                print("image is invalid , please check")
                sys.exit(1)
        else:
            print("image exist ignore,if you want download again,you can delete\n",args.out +'/'+ args.md5_sums)

if __name__ == "__main__":
    main()
