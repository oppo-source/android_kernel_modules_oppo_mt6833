import csv
import subprocess
import json
import os
import xml.etree.ElementTree as ET

class GerritHandler:
    def __init__(self, csv_filename, manifest_path, relative_path):
        self.csv_filename = csv_filename
        self.manifest_path = manifest_path
        self.relative_path = relative_path
        self.root_path = os.getcwd()

    def fetch_target_path(self, project_name):
        tree = ET.ElementTree(file=self.manifest_path)
        for elem in tree.iter(tag='project'):
            if elem.attrib['name'] == project_name:
                return os.path.join(self.root_path, self.relative_path, elem.attrib['path'])
        return None  # 如果找不到对应的项目，返回None

    def run_cmd(self, cmd, target_path=None):
        old_dir = os.getcwd()
        if target_path:
            os.chdir(target_path)
        process = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
        out, err = process.communicate()
        os.chdir(old_dir)
        return out.decode().strip()

    def fetch_commit_ids(self):
        commit_ids = []
        with open(self.csv_filename, 'r') as f:
            reader = csv.reader(f)
            for row in reader:
                split_row = row[0].split("/")
                commit_id = split_row[-2] if split_row[-1] == '' else split_row[-1]
                commit_ids.append(commit_id)
        return commit_ids

    def fetch_json_data(self, commit_id):
        cmd = 'ssh -p 29418 zuoyonghua@gerrit_url gerrit query --format=JSON --patch-sets change:{}'.format(commit_id)
        result = self.run_cmd(cmd)
        try:
            json_results = [json.loads(i) for i in result.splitlines()]
            json_data = json_results[0]
        except IndexError:
            print("No data found for commit id: {}".format(commit_id))
            json_data = None
        return json_data

    def generate_git_command(self, json_data):
        change_number = json_data['number']
        revision_number = json_data['patchSets'][-1]['number']
        project_name = json_data['project']
        git_cmd = 'git fetch ssh://zuoyonghua@gerrit_url:29418/{} refs/changes/{}/{}/{} && git cherry-pick FETCH_HEAD'.format(project_name, str(change_number)[-2:], change_number, revision_number)
        return git_cmd, project_name

    def run_git_command(self, git_cmd, project_name):
        target_path = self.fetch_target_path(project_name)
        if target_path:
            self.run_cmd(git_cmd, target_path)
        else:
            print("Could not find path for project: {}. Skipping...".format(project_name))

    def run(self):
        commit_ids = self.fetch_commit_ids()
        for commit_id in commit_ids:
            print("Processing commit id: {}".format(commit_id))
            json_data = self.fetch_json_data(commit_id)
            if json_data:
                git_cmd, project_name = self.generate_git_command(json_data)
                self.run_git_command(git_cmd, project_name)
                print("Done with commit id: {}".format(commit_id))
            else:
                print("Skipping commit id: {}".format(commit_id))

if __name__ == '__main__':
    gerrit_handler = GerritHandler('git_cherry_pick.csv', '.repo/manifest.xml', '')
    gerrit_handler.run()