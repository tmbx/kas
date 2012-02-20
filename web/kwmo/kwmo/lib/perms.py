import logging

log = logging.getLogger(__name__)

# KWMO permission exceptions
class KWMOPermissionsBadTargetException(Exception):
    pass

# KWMO permissions class
class KWMOPermissions():
    def __init__(self):
        self.object_version = 1                     # Object version
        self.update_version = 0                     # Incremented at every change or changeset.
        self.dirty = False
        self._rules = []                            # Rules store.
        self._roles =   {
                            'root' : \
                            [
                                'a:users',
                                'a:chat',
                                'a:kfs',
                                'a:vnc'
                            ],

                            'roadmin' : \
                            [
                                'a:users.list',
                                'a:chat.list',
                                'a:kfs.list',
                                'a:kfs.download',
                                'a:vnc.list'
                            ],

                            'normal' : \
                            [
                                'a:users.list',
                                'a:chat.list.channel.0',
                                'a:chat.post.channel.0',
                                'a:kfs.list.share.0',
                                'a:kfs.upload.share.0',
                                'a:kfs.download.share.0',
                                'a:vnc.list',
                                'a:vnc.connect'
                            ],

                            'skurl' : \
                            [
                                'a:users.list',
                                'a:pubws.req',
                                'a:kfs.list.share.0'
                            ],

                            'freeze' : \
                            [
                                'd:chat.post',
                                'd:kfs.upload',
                                'd:pubws.req'
                            ] 
                        }

    # Clear permission rules.
    def clear(self, update=True):
        self._rules = []
        if update: self.update_version += 1
        self.dirty = True

    # Add a role.
    def addRole(self, role, update=True):
        if role in self._roles: self._rules += ['r:'+role]
        else: raise Exception("Role '%s' does not exist." % ( str(role) ) )
        if update: self.update_version += 1
        self.dirty = True

    # Delete a role. Do not complain if role is not set.
    def dropRole(self, role, update=True):
        try:
            while 1: self._rules.remove('r:'+role)
        except ValueError:
            pass
        if update: self.update_version += 1
        self.dirty = True

    # Allow single permission.
    def allow(self, perm_name, force=False, update=True):
        if force or not self.hasPerm(perm_name): self._rules += ['a:'+perm_name]
        if update: self.update_version += 1
        self.dirty = True

    # Deny single permission.
    def deny(self, perm_name, force=False, update=True):
        if force or self.hasPerm(perm_name): self._rules += ['d:'+perm_name]
        if update: self.update_version += 1
        self.dirty = True

    # Check for permission.
    # Rules (single rules and roles) are checked in order.
    # Deny rules apply immediately.
    def hasPerm(self, perm_name):
        if self._ruleListCheck(self._rules, perm_name) > 0: return True
        return False

    # Export object state to a dictionary.
    def to_dict(self):
        return { "object_version" : self.object_version, "update_version" : self.update_version, "rules" : self._rules }

    # Import object state from a dictionary.
    def from_dict(self, d):
        self.object_version = d['object_version']
        self.update_version = d['update_version']
        self._rules = d['rules']

    # Check if role is set (currently, this checks the first level
    # but does not check roles defined by other roles).
    def hasRole(self, role_name):
       return self._ruleRoleCheck(self._rules, role_name)

    # Internal: check if role is set in a set of rules (first level only).
    def _ruleRoleCheck(self, rules, role_name):
        if not role_name in self._roles.keys():
            raise Exception("Role '%s' does not exist." % ( role_name ) )
        for rule in rules:
            (rule_type, rule_value) = rule.split(':')
            
            if rule_type == 'r' and rule_value == role_name:
                return True
            
        return False
            
    # Internal: returns permission status from a list of rules.
    def _ruleListCheck(self, rules, perm_name):
        res = 0

        for rule in rules:
            (rule_type, rule_value) = rule.split(':')

            if rule_type == 'r':
                # Role (list of rules)
                role_name = rule_value
                tmp_res = self._roleCheck(role_name, perm_name)

            elif rule_type == 'a' or rule_type == 'd':
                # Single rule
                tmp_res = self._ruleCheck(rule_type, rule_value, perm_name)

            else:
                raise Exception("Bad rule type: rule is: '%s'." % ( str(rule) ) )

            # Use the result if it is authoritative only.
            if tmp_res != 0: res = tmp_res

            # Deny rule takes precedence on allow rules, no matter where (don't check furthur permissions).
            if res < 0: return -1;

        return res

    # Internal: return permission status of a role.
    def _roleCheck(self, role_name, perm_name):
        return self._ruleListCheck(self._roles[role_name], perm_name)

    # Internal: return permission status of a single rule.
    def _ruleCheck(self, rule_type, rule_value, perm_name):
        if perm_name.startswith(rule_value):
            if rule_type == 'a': return 1
            elif rule_type == 'd': return -1;
        else: return 0

# Non-exhaustive tests
def kwmopermissions_test():

    a = KWMOPermissions()

if __name__ == '__main__':
    kwmopermissions_test()

