import os
import requests
from urllib.parse import urljoin
import subprocess
import gzip
import shutil
from datetime import datetime
import csv
import argparse
import re
import sys

kernel_info = []
lts_config_url=(
"xxx/aosp-gki-image-local/"
"android_common_kernel/ack_lts_spl_compliance_schedule.csv")

tools_dir = os.path.dirname(os.path.realpath(__file__))

def parse_cmd_args():
    parser = argparse.ArgumentParser(description="ack lts check")
    parser.add_argument('-b', '--bootimg', type=str, help='Path to boot image', default='boot.img')
    parser.add_argument('-k', '--kernelinfo', type=str, help='Kernel version info file path', default='kernel_version.txt')
    parser.add_argument('-o', '--out', type=str, help='Kernel tmp out dir', default='out')
    parser.add_argument('-u', '--url', type=str, help='ack lts config url', default=lts_config_url)
    parser.add_argument('-c', '--config', type=str, help='ack lts config url download name', default=tools_dir + '/ack_lts_spl_compliance_schedule.csv')
    parser.add_argument('-p', '--prj', type=str, help='ack lts check project', default='0')
    parser.add_argument('-z', '--lz4', type=str, help='lz4 process tool', default=tools_dir + '/lz4')
    parser.add_argument('-n', '--unpack_bootimg', type=str, help='boot unpack tool', default=tools_dir + '/unpack_bootimg')
    args = parser.parse_args()
    print('boot:', args.bootimg, '\nKernel info:', args.kernelinfo, '\nout:', args.out,
      '\nconfig url:', args.url, '\nconfig save:', args.config,
      '\nprj:', args.prj, '\nlz4:', args.lz4,'\nunpack_bootimg:', args.unpack_bootimg)
    return args

def download_compile_ini(bootimg, out):
    try:
        response = requests.get(urljoin(bootimg, 'compile.ini'), stream=True)
    except ImportError:
        print("SSL module is not available. Can't connect to HTTPS URL.")
        return None
    except Exception as e:
        print('An unexpected error occurred:', str(e))
        return None
    if response.status_code != 200:
        print("Server can't connect, please set bootimg and try again")
        return None

    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, 'compile.ini'), 'wb') as f:
        f.write(response.content)
    return response.text.replace("\r\n", "\n").replace("\r", "\n")

def download_boot_img_from_direct_link(direct_link, out):
    try:
        boot_img_response = requests.get(direct_link, stream=True)
    except ImportError:
        print("SSL module is not available. Can't connect to HTTPS URL.")
        return None
    except Exception as e:
        print('An unexpected error occurred:', str(e))
        return None
    if boot_img_response.status_code != 200:
        print('Could not reach {}, status code: {}. Skipping download.'.format(direct_link,boot_img_response.status_code))
        return None

    print('download from: ', direct_link,'to',out)
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, 'boot.img'), 'wb') as f:
        for chunk in boot_img_response.iter_content(chunk_size=8192):
            if chunk:
                f.write(chunk)

def download_prebuild_image(bootimg, out):
    if not bootimg:
        bootimg = input("Your server address: ")

    compile_ini_text = download_compile_ini(bootimg, out)
    if not compile_ini_text:
        return

    ofp_folder_lines = [line for line in compile_ini_text.split("\n") if "ofp_folder" in line]
    if not ofp_folder_lines:
        print("Didn't find 'ofp_folder' in compile.ini")
        return

    ofp = [line.split('=')[1].strip() for line in compile_ini_text.split('\n') if line.startswith("ofp_folder")][0]
    boot_img_link = urljoin(bootimg, "{}/IMAGES/boot.img".format(ofp))
    print('ofp:', ofp,'\ndownload from:', boot_img_link, 'to', out)
    download_boot_img_from_direct_link(boot_img_link, out)

def copy_boot_img_from_local_path(local_path, out):
    print('copy from:', local_path ,'to', out)
    if not os.path.exists(local_path):
        print("{} File does not exist.ignore".format(local_path))
        return
    os.makedirs(out, exist_ok=True)
    try:
        with open(local_path, 'rb') as src_file:
            with open(os.path.join(out, 'boot.img'), 'wb') as dst_file:
                dst_file.write(src_file.read())
    except FileNotFoundError:
        print("Cannot find file: {local_path}".format(local_path))

def handle_boot_img_path(boot_img_path, out):
    if boot_img_path.startswith('http') and not boot_img_path.endswith('boot.img'):
        download_prebuild_image(boot_img_path, out=out)
    elif boot_img_path.startswith('http') and boot_img_path.endswith('boot.img'):
        download_boot_img_from_direct_link(boot_img_path, out)
    elif not boot_img_path.startswith('http'):
        copy_boot_img_from_local_path(boot_img_path, out)
    else:
        print("Invalid boot image path.")

def unpack_boot_img(unpack_bootimg_path, boot_img_path, output_dir):
    if not os.path.exists(boot_img_path):
        print("{} File does not exist.ignore".format(boot_img_path))
        return
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


def parse_version(version_str):
    if not version_str:
        print("Version string is empty.")
        return ""

    match = re.search(r'(\d+).(\d+).(\d+)', version_str)
    if not match:
        print("Invalid version format.")
        return ""

    CURRENT_KERNEL_VERSION = match.group(0)
    CURRENT_VERSION = int(match.group(1))
    CURRENT_PATCHLEVEL = int(match.group(2))
    CURRENT_SUBLEVEL = int(match.group(3))

    if "-android" in version_str:
        main_version, android_version = version_str.split("-android")
        android_version_parts = android_version.split("-")

        LAUNCHVERSION = int(android_version_parts[0])
        KMI_GENERATION = int(android_version_parts[1])

        COMMIT = 'UNKNOWN'
        match = re.search('-g([^\\-]*)(?:-|$)', android_version)
        if match:
            COMMIT = match.group(1)
    else:
        LAUNCHVERSION = '0'
        KMI_GENERATION = '0'
        COMMIT = 'UNKNOWN'

    return {
            "CURRENT_KERNEL_VERSION": CURRENT_KERNEL_VERSION,
            "CURRENT_VERSION": CURRENT_VERSION,
            "CURRENT_PATCHLEVEL": CURRENT_PATCHLEVEL,
            "CURRENT_SUBLEVEL": CURRENT_SUBLEVEL,
            "LAUNCHVERSION": LAUNCHVERSION,
            "KMI_GENERATION": KMI_GENERATION,
            "COMMIT": COMMIT
    }

def print_version_info(version_info):
    if not version_info:
        print("version_info string is empty.")
        return
    print("CURRENT_KERNEL_VERSION:", version_info["CURRENT_KERNEL_VERSION"])
    print("CURRENT_VERSION:", version_info["CURRENT_VERSION"])
    print("CURRENT_PATCHLEVEL:", version_info["CURRENT_PATCHLEVEL"])
    print("CURRENT_SUBLEVEL:", version_info["CURRENT_SUBLEVEL"])
    print("LAUNCHVERSION:", version_info["LAUNCHVERSION"])
    print("KMI_GENERATION:", version_info["KMI_GENERATION"])
    print("COMMIT:", version_info["COMMIT"])
    update_kernel_info("CURRENT_KERNEL_VERSION:"+version_info["CURRENT_KERNEL_VERSION"])
    update_kernel_info("CURRENT_VERSION:"+str(version_info["CURRENT_VERSION"]))
    update_kernel_info("CURRENT_PATCHLEVEL:"+str(version_info["CURRENT_PATCHLEVEL"]))
    update_kernel_info("CURRENT_SUBLEVEL:"+str(version_info["CURRENT_SUBLEVEL"]))
    update_kernel_info("LAUNCHVERSION:"+str(version_info["LAUNCHVERSION"]))
    update_kernel_info("KMI_GENERATION:"+str(version_info["KMI_GENERATION"]))
    update_kernel_info("COMMIT:"+str(version_info["COMMIT"]))

def get_current_date():
    today = datetime.today()
    formatted_date = today.strftime("%Y%m%d")
    return formatted_date

def check_substring(substring, target_string):
    # Checks if content is not empty, not equal to "0", and not only spaces
    if not substring or substring.strip() == "0" or not substring.strip():
        return "0"

    if not target_string or target_string.strip() == "0" or not target_string.strip():
        return "0"

    # Checks if passed single string is in another string
    sub_list = substring.split('/')
    target_list = target_string.split('/')
    for sub in sub_list:
        if sub.strip() in target_list:
            return "1"

    return "0"

def parse_params(version_info, current_date, csv_file, prj):

    ack_lts_message = "pass"
    ack_lts_error = "0"
    build_trigger_error = "0"
    by_pass_prj_check = "0"
    if os.path.exists(csv_file) and version_info:
        LAUNCHVERSION = version_info["LAUNCHVERSION"]
        CURRENT_VERSION = version_info["CURRENT_VERSION"]
        CURRENT_PATCHLEVEL = version_info["CURRENT_PATCHLEVEL"]
        CURRENT_SUBLEVEL = version_info["CURRENT_SUBLEVEL"]
        KMI_GENERATION = version_info["KMI_GENERATION"]
        COMMIT = version_info["COMMIT"]

        LAUNCHVERSION, CURRENT_VERSION, CURRENT_PATCHLEVEL, CURRENT_SUBLEVEL = map(int, [LAUNCHVERSION, CURRENT_VERSION, CURRENT_PATCHLEVEL, CURRENT_SUBLEVEL])  # Ensure these parameters are integers
        print("Parameters: LAUNCHVERSION={}, CURRENT_VERSION={}, CURRENT_PATCHLEVEL={}, CURRENT_SUBLEVEL={}, current_date={}, csv_file={}\n"
           .format(LAUNCHVERSION, CURRENT_VERSION, CURRENT_PATCHLEVEL, CURRENT_SUBLEVEL, current_date, csv_file))


        with open(csv_file, 'r') as file:
            reader = csv.reader(file)
            next(reader)  # Skip header row
            found = False
            for index, row in enumerate(reader):
                print("Reading row {}: {}".format(index+2, row))  # Print the content of the row (index+2 is the actual row number in CSV)
                if int(row[0]) == LAUNCHVERSION and len(row)==14:
                    LTS_VERSION, LTS_PATCHLEVEL, LTS_SUBLEVEL = map(int, row[2].split('.'))
                    print("\nLTS_VERSION={}, LTS_PATCHLEVEL={}, LTS_SUBLEVEL={}".format(LTS_VERSION, LTS_PATCHLEVEL, LTS_SUBLEVEL))
                    print("CURRENT_VERSION={},CURRENT_PATCHLEVEL={},CURRENT_SUBLEVEL={}\n".format(CURRENT_VERSION, CURRENT_PATCHLEVEL, CURRENT_SUBLEVEL))

                    if CURRENT_VERSION == LTS_VERSION and CURRENT_PATCHLEVEL == LTS_PATCHLEVEL:
                        found = True
                        lts_minium_version = row[2]
                        normal_date = row[3]
                        information = row[4]
                        warning_date = row[5]
                        warning = row[6]
                        error_date = row[7]
                        error = row[8]
                        fatal_date = row[9]
                        fatal = row[10]
                        good = row[11]
                        build_trigger_error = row[12]
                        by_pass_projects = row[13]
                        by_pass_prj_check = check_substring(prj, by_pass_projects)
                        update_info = (  "LTS_SPL_MINIUM_VERSION:" + str(lts_minium_version) +
                                     "\nLTS_VERSION:" + str(LTS_VERSION) +
                                     "\nLTS_PATCHLEVEL:" + str(LTS_PATCHLEVEL) +
                                     "\nLTS_SUBLEVEL:" + str(LTS_SUBLEVEL) +
                                     "\nLTS_SPL_UPCOMING_SCHEDULE_DATE:" + fatal_date + "\n" +
                                     "\nNORMAL_DATE:" + normal_date +
                                     "\nWARNING_DATE:" + warning_date +
                                     "\nERROR_DATE:" + error_date +
                                     "\nFATAL_DATE:" + fatal_date + "\n" +
                                     "\nNORMAL_MESSAGE:" + information +
                                     "\nWARNING_MESSAGE:" + warning +
                                     "\nERROR_MESSAGE:" + error +
                                     "\nFATAL_MESSAGE:" + fatal +
                                     "\nDEFAULT_MESSAGE:" + good+"\n"
                                     "\nBUILD_STATE_CHECK:" + build_trigger_error +
                                     "\nBY_PASS_PROJECTS:" + by_pass_projects +
                                     "\nCURRENT_PROJECT:" + prj +
                                     "\nBY_PASS_CHECK_STATUS:" + by_pass_prj_check+"\n"
                                   )
                        print(update_info)

                        update_kernel_info(update_info)
                        if CURRENT_SUBLEVEL >= LTS_SUBLEVEL:
                            ack_lts_message =  good
                        else:
                            if current_date < normal_date:
                                ack_lts_message = information
                            elif current_date < warning_date:
                                ack_lts_message = warning
                            elif current_date < error_date:
                                ack_lts_message = error
                                ack_lts_error = "1"
                            elif current_date < fatal_date:
                                ack_lts_message = fatal
                                ack_lts_error = "1"
                            else:
                                ack_lts_message = fatal
                                ack_lts_error = "1"
                        break
    update_info = "ACK_LTS_SPL_CHECK:" + ack_lts_message
    print(update_info+"\n")
    update_kernel_info(update_info)
    update_kernel_info("\nack_lts_error:"+ack_lts_error)
    update_kernel_info("build_trigger_error:"+build_trigger_error)
    update_kernel_info("by_pass_prj_check:"+by_pass_prj_check)
    return {
            "ack_lts_error": ack_lts_error,
            "build_trigger_error": build_trigger_error,
            "by_pass_prj_check": by_pass_prj_check
    }


def check_prj_info(parse_info):
    if not parse_info:
        print("parse_info string is empty.")
        return

    ack_lts_error = parse_info["ack_lts_error"]
    build_trigger_error = parse_info["build_trigger_error"]
    by_pass_prj_check = parse_info["by_pass_prj_check"]

    print("ack_lts_error:", parse_info["ack_lts_error"])
    print("build_trigger_error:", parse_info["build_trigger_error"])
    print("by_pass_prj_check:", parse_info["by_pass_prj_check"],"\n")

    if by_pass_prj_check == "1":
       print("check default project by pass check")
       return
    if ack_lts_error == "1" and build_trigger_error == "1":
       print("check lts version error and config build as error")
       sys.exit(1)

def update_kernel_info(new_info):
    global kernel_info
    kernel_info.append(new_info)

def write_info_to_file(path):
    global kernel_info
    dir_name = os.path.dirname(path)
    if dir_name and not os.path.exists(dir_name):
        os.makedirs(dir_name)
    with open(path, 'w') as f:
        f.write("\n".join(kernel_info))

def download_config_file(output_file,url):
    try:
        try:
            response = requests.get(url, stream=True)
        except ImportError:
            print("SSL module is not available. Can't connect to HTTPS URL.")
            return None
        if response.status_code != 200:
            print('Could not reach {}, status code: {}. Skipping download.'.format(url,response.status_code))
            return None
        try:
            subprocess.run(["curl", "-o", output_file, url], check=True)
            print('{} Downloaded successfully.'.format(url))
        except subprocess.CalledProcessError:
            print("Error occurred while downloading the file.")
            return None
    except Exception as e:
        print('An unexpected error occurred:', str(e))
        return None

def append_csv_to_txt(csv_file, txt_file):
    if not os.path.exists(csv_file):
        print("{} File does not exist.ignore".format(csv_file))
        return
    try:
        with open(csv_file, 'r') as csv:
            data = csv.read()

        with open(txt_file, 'a') as txt:
            txt.write(data)

    except Exception as e:
        print('Error occurred: {}'.format(e))

def delete_files_and_dirs(path, delete_list):
    # Ensure the provided directory exists
    if not os.path.exists(path):
        print('The provided directory does not exist.')
        return

    # Iterate over the list of files/directories to delete
    for item in delete_list:
        item_path = os.path.join(path, item)
        # If the item exists, delete it
        if os.path.exists(item_path):
            # If it's a directory, use shutil.rmtree() to delete it
            if os.path.isdir(item_path):
                shutil.rmtree(item_path)
            # If it's a file, use os.remove() to delete it
            else:
                os.remove(item_path)

    # If the directory is empty after deleting the specified files and directories, delete the directory
    if not os.listdir(path):
        os.rmdir(path)

print('\nstep 1 get input argc/argv')
args = parse_cmd_args()

bootimg = os.path.join(args.out, "boot.img")
unpack = os.path.join(args.out, "unpack")
kernel = os.path.join(unpack, "kernel")

print('\nstep 2 download image')
handle_boot_img_path(args.bootimg,args.out)

print("\nstep 3 uppack boot.img")
unpack_boot_img(args.unpack_bootimg, bootimg, unpack)

print("step 4 get kernel compression type and decompress")
compression_type = get_compression_type(kernel,args.lz4)
print(compression_type)

try:
    print("\nstep 5 get kernel linux kernel version")
    version = find_linux_versions(kernel)
    update_info = "LINUX_KERNEL_FULL_VERSION:"+version+"\n"
    print(update_info)
    update_kernel_info(update_info)
except ValueError as e:
    print(str(e))

print("\nstep 6 parse kernel version")
version_info = parse_version(version)
print_version_info(version_info)

print("\nstep 7 get current date")
current_date = get_current_date()
update_info = "\nLINUX_KERNEL_BUILD_DATE:"+current_date+"\n"
print(update_info)
update_kernel_info(update_info)

print("\nstep 8 download config csv file")
download_config_file(args.config,args.url)

print("\nstep 9 parse ack lts schedule params")
parse_info = parse_params(version_info, current_date, args.config, args.prj)
print("\nstep 10 save kernel info\n")
update_kernel_info("\n")
write_info_to_file(args.kernelinfo)
append_csv_to_txt(args.config,args.kernelinfo)

delete_list = ['boot.img', 'compile.ini', 'unpack']
delete_files_and_dirs(args.out, delete_list)

print("\nstep 11 check build result\n")
check_prj_info(parse_info)