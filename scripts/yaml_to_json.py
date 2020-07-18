import sys
import yaml
import json

filename = sys.argv[1]
indentations = 2
with open(filename, 'r') as yaml_file:
    data = yaml.load(yaml_file, Loader=yaml.FullLoader)
    json_data = json.dumps(data, indent=indentations)
with open(filename.replace('.yaml', '.json').replace('.yml', 'json'), 'w') as json_file:
    json_file.write(json_data)
