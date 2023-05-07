# gp_scripts_ext samba gpo policy
# Copyright (C) David Mulder <dmulder@suse.com> 2020
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import os, re
from subprocess import Popen, PIPE
from samba.gp.gpclass import gp_pol_ext, drop_privileges, gp_file_applier
from hashlib import blake2b
from tempfile import NamedTemporaryFile
from samba.gp.util.logging import log

intro = '''
### autogenerated by samba
#
# This file is generated by the gp_scripts_ext Group Policy
# Client Side Extension. To modify the contents of this file,
# modify the appropriate Group Policy objects which apply
# to this machine. DO NOT MODIFY THIS FILE DIRECTLY.
#

'''
end = '''
### autogenerated by samba ###
'''

class gp_scripts_ext(gp_pol_ext, gp_file_applier):
    def __str__(self):
        return 'Unix Settings/Scripts'

    def process_group_policy(self, deleted_gpo_list, changed_gpo_list, cdir=None):
        for guid, settings in deleted_gpo_list:
            if str(self) in settings:
                for attribute, script in settings[str(self)].items():
                    self.unapply(guid, attribute, script)

        for gpo in changed_gpo_list:
            if gpo.file_sys_path:
                reg_key = 'Software\\Policies\\Samba\\Unix Settings'
                sections = { '%s\\Daily Scripts' % reg_key : '/etc/cron.daily',
                             '%s\\Monthly Scripts' % reg_key : '/etc/cron.monthly',
                             '%s\\Weekly Scripts' % reg_key : '/etc/cron.weekly',
                             '%s\\Hourly Scripts' % reg_key : '/etc/cron.hourly' }
                pol_file = 'MACHINE/Registry.pol'
                path = os.path.join(gpo.file_sys_path, pol_file)
                pol_conf = self.parse(path)
                if not pol_conf:
                    continue
                policies = {}
                for e in pol_conf.entries:
                    if e.keyname in sections.keys() and e.data.strip():
                        if e.keyname not in policies:
                            policies[e.keyname] = []
                        policies[e.keyname].append(e.data)
                def applier_func(keyname, entries):
                    ret = []
                    cron_dir = sections[keyname] if not cdir else cdir
                    for data in entries:
                        with NamedTemporaryFile(prefix='gp_', mode="w+",
                                                delete=False, dir=cron_dir) as f:
                            contents = '#!/bin/sh\n%s' % intro
                            contents += '%s\n' % data
                            f.write(contents)
                            os.chmod(f.name, 0o700)
                            ret.append(f.name)
                    return ret
                for keyname, entries in policies.items():
                    # Each GPO applies only one set of each type of script, so
                    # so the attribute matches the keyname.
                    attribute = keyname
                    # The value hash is generated from the script entries,
                    # ensuring any changes to this GPO will cause the scripts
                    # to be rewritten.
                    value_hash = self.generate_value_hash(*entries)
                    self.apply(gpo.name, attribute, value_hash, applier_func,
                               keyname, entries)

                # Cleanup any old scripts that are no longer part of the policy
                self.clean(gpo.name, keep=policies.keys())

    def rsop(self, gpo, target='MACHINE'):
        output = {}
        pol_file = '%s/Registry.pol' % target
        if gpo.file_sys_path:
            path = os.path.join(gpo.file_sys_path, pol_file)
            pol_conf = self.parse(path)
            if not pol_conf:
                return output
            for e in pol_conf.entries:
                key = e.keyname.split('\\')[-1]
                if key.endswith('Scripts') and e.data.strip():
                    if key not in output.keys():
                        output[key] = []
                    output[key].append(e.data)
        return output

def fetch_crontab(username):
    p = Popen(['crontab', '-l', '-u', username], stdout=PIPE, stderr=PIPE)
    out, err = p.communicate()
    if p.returncode != 0:
        log.warning('Failed to read the crontab: %s' % err)
    m = re.findall('%s(.*)%s' % (intro, end), out.decode(), re.DOTALL)
    if len(m) == 1:
        entries = m[0].strip().split('\n')
    else:
        entries = []
    m = re.findall('(.*)%s.*%s(.*)' % (intro, end), out.decode(), re.DOTALL)
    if len(m) == 1:
        others = '\n'.join([l.strip() for l in m[0]])
    else:
        others = out.decode()
    return others, entries

def install_crontab(fname, username):
    p = Popen(['crontab', fname, '-u', username], stdout=PIPE, stderr=PIPE)
    _, err = p.communicate()
    if p.returncode != 0:
        raise RuntimeError('Failed to install crontab: %s' % err)

class gp_user_scripts_ext(gp_scripts_ext):
    def process_group_policy(self, deleted_gpo_list, changed_gpo_list):
        for guid, settings in deleted_gpo_list:
            self.gp_db.set_guid(guid)
            if str(self) in settings:
                others, entries = fetch_crontab(self.username)
                for attribute, entry in settings[str(self)].items():
                    if entry in entries:
                        entries.remove(entry)
                    self.gp_db.delete(str(self), attribute)
                with NamedTemporaryFile() as f:
                    if len(entries) > 0:
                        f.write('\n'.join([others, intro,
                                   '\n'.join(entries), end]).encode())
                    else:
                        f.write(others.encode())
                    f.flush()
                    install_crontab(f.name, self.username)
            self.gp_db.commit()

        for gpo in changed_gpo_list:
            if gpo.file_sys_path:
                reg_key = 'Software\\Policies\\Samba\\Unix Settings'
                sections = { '%s\\Daily Scripts' % reg_key : '@daily',
                             '%s\\Monthly Scripts' % reg_key : '@monthly',
                             '%s\\Weekly Scripts' % reg_key : '@weekly',
                             '%s\\Hourly Scripts' % reg_key : '@hourly' }
                self.gp_db.set_guid(gpo.name)
                pol_file = 'USER/Registry.pol'
                path = os.path.join(gpo.file_sys_path, pol_file)
                pol_conf = drop_privileges('root', self.parse, path)
                if not pol_conf:
                    continue
                for e in pol_conf.entries:
                    if e.keyname in sections.keys() and e.data.strip():
                        cron_freq = sections[e.keyname]
                        attribute = '%s:%s' % (e.keyname,
                                blake2b(e.data.encode()).hexdigest())
                        old_val = self.gp_db.retrieve(str(self), attribute)
                        entry = '%s %s' % (cron_freq, e.data)
                        others, entries = fetch_crontab(self.username)
                        if not old_val or entry not in entries:
                            entries.append(entry)
                            with NamedTemporaryFile() as f:
                                f.write('\n'.join([others, intro,
                                           '\n'.join(entries), end]).encode())
                                f.flush()
                                install_crontab(f.name, self.username)
                            self.gp_db.store(str(self), attribute, entry)
                        self.gp_db.commit()

    def rsop(self, gpo):
        return super().rsop(gpo, target='USER')
