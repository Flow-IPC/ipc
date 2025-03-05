
#!/usr/bin/env python3

import argparse
import subprocess
import pathlib
import tempfile
import ruamel.yaml


def create_modified_workflow(workflow_path, cxx_std, compiler, build_test_cfg):
  with open(workflow_path, 'r') as file:
      data = ruamel.yaml.safe_load(file)

  if "needs.setup.outputs.proceed-else-not == 'true'" in data['jobs']['build-and-test']['if']:
    del data['jobs']['build-and-test']['if']
  if "needs.setup.outputs.proceed-else-not == 'true'" in data['jobs']['doc-and-release']['if']:
    del data['jobs']['doc-and-release']['if']

  cxx_std = [ next(comp for comp in data['jobs']['build-and-test']['strategy']['matrix']['cxx-std'] if comp['id'] == cxx_std) ]
  compiler = [ next(comp for comp in data['jobs']['build-and-test']['strategy']['matrix']['compiler'] if comp['id'] == compiler) ]
  build_test_cfg = [ next(cfg for cfg in data['jobs']['build-and-test']['strategy']['matrix']['build-test-cfg'] if cfg['id'] == build_test_cfg) ]

  data['jobs']['build-and-test']['strategy']['matrix'] = {
       'cxx-std': cxx_std,
       'compiler': compiler,
       'build-test-cfg': build_test_cfg
  }

  tweaked_workflow = tempfile.NamedTemporaryFile(suffix='.yml', mode='w', encoding='utf-8', delete=False)
  ruamel.yaml.safe_dump(data, tweaked_workflow)
  print(tweaked_workflow.name)
  return tweaked_workflow


def run_act_dryrun(workflow_path):
  subprocess.run(f"act --dryrun -W {workflow_path}", shell=True, capture_output=False, text=True)


def main():
  parser = argparse.ArgumentParser(description='Run "act --dryrun" to validate IPC workflow correctness.')
  parser.add_argument('workflow', type=pathlib.Path, help='Path to the workflow YAML file.')
  parser.add_argument('-s', '--cxx-std', type=str, help='Specify one from possible values for the cxx-std from the workflow file.', default='cxx17')
  parser.add_argument('-c', '--compiler', type=str, help='Specify one from possible values for the compiler from the workflow file.', default='gcc-9')
  parser.add_argument('-b', '--build-test-cfg', type=str, help='Specify one from possible values for the compiler from the workflow file.', default='debug')
  args = parser.parse_args()

  tweaked_workflow = create_modified_workflow(workflow_path=args.workflow, cxx_std=args.cxx_std, compiler=args.compiler, build_test_cfg='debug')
  run_act_dryrun(workflow_path=tweaked_workflow.name)


if __name__ == "__main__":
  main()