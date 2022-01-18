# Deploy firmware

This playbook will deploy the firmware files to servers running on elastic beanstalk.

The servers are loaded by the dynamic aws inventory file `inventory/aws_ec2.yml`.


# Usage

(If you want to run the playbook locally you can use the pipenv from the ../runner directory. Move to that directory
and activate the shell using `pipenv shell`. You also need rsync installed locally.)

Deploy to staging
```
ansible-playbook -i inventory deploy-firmware -e firmware_src_dir=/path/to/built/firmware -e firmware_install_dir=/mnt/efs/firmware --limit staging
```

(The hosts are placed into groups in the dynamic inventory file, and the deploy can be limited to one or more groups
by the --limit argument.



