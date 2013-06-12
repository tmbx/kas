import os, types
from kfile import read_file, write_file_atom, read_ini_file, write_ini_file
from kodict import odict
from kproperty import PropContainer, PropSet, PropModel, Prop, IntProp, LongProp, StrProp

# Convert int object to long, if possible. Otherwise, return value as-is.
def convert_int_to_long(value):
    if isinstance(value, int): return long(value)
    return value

# Enclose a string with single quotes. Backslashes and single quotes are escaped
# with a backslash.
def escape_string(value):
    return "'" + value.replace("\\", "\\\\").replace("'", "\\'") + "'"

# Represents a configuration node that contains configuration properties.
class AbstractConfigNode(PropContainer):
    # Import configurations from a kserialized python object.
    def load_from_kserialized_obj(self, data, update=False):
        for name, prop in self.prop_set.items():

            if not update:
                # Reset property value.
                prop.reset(self)

            for key, value in data:
                # Walk through key-value tuples.

                if key == name:
                    # Found a record that matches current property.
                    if isinstance(value, list):
                        # Get the property list object.
                        prop_list = prop.__get__(self)

                        # Import list data.
                        prop_list.load_from_kserialized_obj(value, update)

                    elif isinstance(value, tuple):
                        # Get the property dict object.
                        prop_dict = prop.__get__(self)

                        # Import dict data.
                        prop_dict.load_from_kserialized_obj(value, update)
                    else:

                        # Set property value directly.
                        prop.__set__(self, value)

# Validating list object.
class ModeledList(list):
    def __init__(self, value_model, *args, **kwargs):
        # Get value model.
        if type(value_model) != types.TypeType:
           raise Exception("%s: bad value_model (not instantiable): '%s'" % 
                           ( self.__class__.__name__, str(value_model) ) )
        self.value_model = value_model

        # Get value import callable.
        self.import_value_call = None
        if kwargs.has_key('import_value_call'):
            if not callable(kwargs['import_value_call']):
                raise Exception("import_value_call parameter is not callable.")
            self.import_value_call = kwargs['import_value_call']
            del kwargs['import_value_call']

        # Super.
        list.__init__(self, *args, **kwargs)

        # Replace list elements with validated ones.
        for i in range(0, len(self)):
            self[i] = self._validate_value(self[i])

    # Validate value.
    def _validate_value(self, value):
        # Import value, if needed.
        if self.import_value_call:
            value = self.import_value_call(value)

        # Validate value.
        if not isinstance(value, self.value_model):
            raise Exception("Value '%s' is not an instance of model '%s'." % ( str(value), str(self.value_model) ) )

        return value

    def __setitem__(self, key, value):
        # Validate value.
        value = self._validate_value(value)

        # Set value.
        list.__setitem__(self, key, value)

    def append(self, value, *args, **kwargs):
        # Validate value.
        value = self._validate_value(value)

        # Append value.
        list.append(self, value, *args, **kwargs)

    # Update list with another list.
    def import_data(self, data):
        try:
            # Empty list.
            while 1:
                self.pop()
        except IndexError:
            pass
        for item in data:
            self.append(item)

# Config list.
class ConfigList(ModeledList):
    # Import list items from a kserialized python object.
    def load_from_kserialized_obj(self, data, update):
        # Delete all list elements.
        for i in range(0, len(self)):
            self.pop()

        for value in data:
            # Instantiate model.
            obj = self.value_model()
            try:
                # Load a kserialized object.
                f = getattr(obj, 'load_from_kserialized_obj')
                f(value, update)
            except AttributeError:
                # Load a regular value.
                obj = value
            self.append(obj)

# Validating and ordered dict object.
class ModeledDict(odict):
    def __init__(self, key_model, value_model, *args, **kwargs):
        # Get key model.
        if type(key_model) != types.TypeType:
           raise Exception("%s: bad key_model (not instantiable): '%s'" % \
                ( self.__class__.__name__, str(key_model) ) )
        self.key_model = key_model

        # Get value model.
        if type(value_model) != types.TypeType:
           raise Exception("%s: bad value_model (not instantiable): '%s'" % \
                ( self.__class__.__name__, str(value_model) ) )
        self.value_model = value_model

        # Get key import callable.
        self.import_key_call = None
        if kwargs.has_key('import_key_call'):
            if not callable(kwargs['import_key_call']):
                raise Exception("import_key_call parameter is not callable.")
            self.import_key_call = kwargs['import_key_call']
            del kwargs['import_key_call']

        # Get value import callable.
        self.import_value_call = None
        if kwargs.has_key('import_value_call'):
            if not callable(kwargs['import_value_call']):
                raise Exception("import_value_call parameter is not callable.")
            self.import_value_call = kwargs['import_value_call']
            del kwargs['import_value_call']

        # Super.
        odict.__init__(self, *args, **kwargs)

        # Replace keys and values with validated ones.
        for key, value in self.items():
            key = self._validate_key(key)
            value = self._validate_value(value)
            self[key] = value

    # Validate key.
    def _validate_key(self, key):
        # Import key, if needed.
        if self.import_key_call:
            key = self.import_key_call(key)

        # Validate key.
        if not isinstance(key, self.key_model):
            raise Exception("Key '%s' is not an instance of model '%s'." % ( str(key), str(self.key_model) ) )

        return key

    # Validate value.
    def _validate_value(self, value):
        # Import value, if needed.
        if self.import_value_call:
            value = self.import_value_call(value)

        # Validate value.
        if not isinstance(value, self.value_model):
            raise Exception("Value '%s' is not an instance of model '%s'." % ( str(value), str(self.value_model) ) )

        return value

    def __setitem__(self, key, value):
        # Validate items.
        key = self._validate_key(key)
        value = self._validate_value(value)

        # Set value.
        odict.__setitem__(self, key, value)

    # Update dict with another dict.
    def import_data(self, data):
        self.clear()
        for key, value in data.items():
            self[key] = value

# Config dictionary.
class ConfigDict(ModeledDict):
    # Import dict items from a kserialized python object.
    # Input data is key-value pairs.
    def load_from_kserialized_obj(self, data, update):
        # Delete all dict elements.
        self.clear()

        for key, value in data:
            # Instantiate object.
            obj = self.value_model()

            try:
                # Load a kserialized object.
                f = getattr(obj, 'load_from_kserialized_obj')
                f(value, update)
            except AttributeError:
                # Load a regular value.
                obj = value
            self[key] = obj

    def __setitem__(self, obj, value):
        # Convert value from unicode to latin1 before setting it, if needed.
        if isinstance(value, unicode):
            value = value.encode('latin1')

        # Super
        ModeledDict.__setitem__(self, obj, value)

# Property set.
class ConfigPropSet(PropSet):
    # Add a property.
    def add_prop(self, name, value, doc=''):
        if isinstance(value, basestring):
            self[name] = StrProp(default=value, doc=doc)
        elif isinstance(value, int):
            self[name] = IntProp(default=value, doc=doc, convert_from_string=True)
        elif isinstance(value, long):
            self[name] = LongProp(default=value, doc=doc, convert_from_string=True)
        elif isinstance(value, Prop):
            self[name] = value
        elif type(value) == types.TypeType:
            self[name] = Prop(model = PropModel(cls=value), doc=doc)
        else:
            raise Exception("invalid type for %s in property set" % (name))

# Recursively dump config to a kserialized string, with comments. Output is then
# parsable by python (eval) and can be imported again.
class Dumper(object):

    # Object used to pass simple values by reference (int, string, ...).
    class RefObj(object):
        comment = None

    def __init__(self):
        self.reset()

    # Reset state.
    def reset(self):
        self.buffer = ''
        self.padding = 0
        self.line_length = 0

    # Write data to buffer.
    def write(self, data):
        # Calculate the line length. Newlines reset the line length.
        rf = data.rfind('\n')
        if rf > -1: self.line_length = len(data) - (rf + 1)
        else: self.line_length += len(data)

        # Write data to buffer.
        self.buffer += data

    # Change line and add padding. 
    def change_line(self, padding=None):
        if not padding: padding = self.padding
        self.write('\n' + padding * ' ')

    # Dump formatted comment.
    def format_comment(self, comment):
        
        # The comment text length is the the terminal width minus the line
        # length, the " # " string length and a space at the end of the line.
        avail_space = 80 - self.line_length - 4
        max_comment_length = max(avail_space, 40)

        if self.line_length > avail_space:
            comment_on_same_line = False
            padding = 8
        else:
            comment_on_same_line = True
            padding = self.line_length
        
        # Cut comment in lines not longer than <max_comment_length>, if needed.
        lines = []
        cur_line = ""
        for word in comment.split():
            if cur_line == "":
                cur_line = word
            else:
                if len(cur_line) + 1 + len(word) > max_comment_length:
                    lines.append(cur_line)
                    cur_line = word
                else:
                    cur_line += " " + word
        if cur_line != "": lines.append(cur_line)
        
        # Bail out, no lines.
        if not len(lines): return

        # Change line.
        if not comment_on_same_line: self.change_line(padding)
        
        # Dump the first line.
        self.write(" # " + lines[0])
        
        # Dump the subsequent lines.
        for line in lines[1:]:
            self.write('\n' + padding * ' ' + " # " + line)
                
    # Recursively dump config to a kserialized string, with comments. Output is
    # then parsable by python (eval) and can be imported again.
    def dump_config_to_kserialized_string(self, obj, level=0, ref_obj=RefObj()):
        # Reset state on first level call.
        if level == 0: self.reset()
            
        comment = None

        # Default delimiting characters.
        container_start_char = '('
        container_stop_char = ')'

        # Extract data from object.
        if isinstance(obj, AbstractConfigNode):
            length = len(obj.prop_set)
            keys = obj.prop_set.keys()
            values = obj.prop_set.values()

        elif isinstance(obj, dict):
            length = len(obj)
            keys = obj.keys()
            values = obj.values()

        elif isinstance(obj, list):
            length = len(obj)
            keys = None
            values = obj
            container_start_char = '['
            container_stop_char = ']'

        else:
            raise Exception("Object not supported.")

        # Start dump.
        self.padding += 1
        self.write(container_start_char)

        for i in range(0, length):
            # Get next key if any.
            key = None
            if keys: key = keys[i]

            # Get next value.
            value = values[i]

            comment = None

            if isinstance(value, Prop):
                # Get property comment.
                comment = value.doc

                # Get real value from property.
                value = value.__get__(obj)

            if key:
                # Dictionary-like - begin tuple.
                self.padding += 1
                self.write('(')

                # Write key.
                if isinstance(key, basestring): self.write(escape_string(key) + ',')
                elif isinstance(key, int) or isinstance(key, long): self.write(str(key) + ',')
                else: raise Exception("Unsupported key type.")

            if isinstance(value, basestring):
                cur_len = len(value)
                if key: cur_len += len(str(key))
                if cur_len > 60: self.change_line()

                # Write string.
                if key: self.write(' ')
                lines = value.split('\n')
                if len(lines) > 1  and lines[-1] == '': lines = lines[:-1]
                count = 0
                for line in lines:
                    count += 1
                    if count < len(lines):
                        line = line + '\n'
                    if count > 1:
                        self.write(' ' + escape_string(line))
                    else:
                        self.write(escape_string(line))
                    if count < len(lines):
                        self.write(' + \\')
                        self.change_line()

            elif isinstance(value, int) or isinstance(value, long) or isinstance(value, float):
                # Write raw value (same line).
                if key: self.write(' ')
                self.write(str(value))

            else:
                if key:
                    # Dictionary-like value.
                    if comment != None:
                        # Write comment (same line).
                        self.format_comment(comment)

                    # Change line.
                    self.change_line()

                # Get value dump and last comment.
                self.dump_config_to_kserialized_string(value, level + 1, ref_obj)
                comment = ref_obj.comment

            if key:
                # Dictionary-like - end tuple.
                self.padding -= 1
                self.write(')')

            # Add separating comma (needed for tuples, at least when only one
            # element is set... supported in dict and list for every element).
            if (i + 1) < length or (key and length == 1):
                self.write(',')
            
            # Handle intermediate line.
            if (i + 1) != length:
                # Write comment.
                if comment != None: self.format_comment(comment)

                # Change line.
                self.change_line()

        # End dump.
        self.padding -= 1
        self.write(container_stop_char)

        # Remember last comment.
        ref_obj.comment = comment

        if level == 0:
            if comment != None:
                # Write last comment.
                self.format_comment(comment)
                self.write("\n")

            return self.buffer

# Convert master config to regular python format.
def convert_mc_to_python(data):
    if isinstance(data, AbstractConfigNode):
        d = odict()
        for key, prop in data.prop_set.items():
            d[key] = convert_mc_to_python(prop.__get__(data))
        return d
    if isinstance(data, tuple):
        d = odict()
        for key, value in data:
            d[key] = convert_mc_to_python(value)
        return d
    elif isinstance(data, list):
        l = []
        for item in data:
            l.append(convert_mc_to_python(item))
        return l
    else:
        return data

# Convert python format to master config format.
def convert_python_to_mc(data):
    if isinstance(data, dict):
        l = []
        for key, value in data.items():
            l.append((key, convert_python_to_mc(value)))
        return tuple(l)
    elif isinstance(data, list):
        l = []
        for item in data:
            l.append(convert_python_to_mc(item))
        return l
    else:
        return data

# INI config value.
class KIniValue(object):
    def __init__(self, value='', doc=None):
        if not (isinstance(value, basestring) or isinstance(value, int) or  isinstance(value, long) or isinstance(value, float)):
            raise Exception("KIniValue: '%s' is not a valid value." % ( str(value) ) )
        self.value = value
        self.doc = doc

# INI config section.
class KIniSection(odict):
    def __init__(self, doc=None):
        odict.__init__(self)
        self.doc = doc

# INI config file.
class KIniFile(object):
    def __init__(self, doc=None):
        self.doc = doc
        self.sections = odict()

    # Format comments so that lines longer than 115 characters are split, and
    # insert '# ' before every line.
    # Note: Use comments that already contain \n to avoid this method cutting
    # words appropriately.
    def format_comment(self, comment):
        max_length = 115
        lines = []
        arr = comment.split('\n')
        for line in arr:
            while len(line) >= max_length:
                lines.append(line[:max_length])
                line = line[max_length:]
            lines.append(line)
        return ''.join(map(lambda x: "# "+x+'\n', lines))

    # Add a section. 
    def add_section(self, name, section=None, doc=None):
        # Make sure section doesn't already exist.
        if name in self.sections.keys():
            raise Exception("Section '%s' already exists." % ( name ) )

        # Make sure section is a KIniSection object.
        if section == None:
            section = KIniSection()
        elif not (isinstance(section, KIniSection)):
            raise Exception("Section value is not an instance of the KIniSection class.")

        # Set section doc if needed.
        if doc: section.doc = doc

        #  Add the section.
        self.sections[name] = section

    # Set a value.
    def set(self, sname, key, value='', doc=None):
        # Make sure the section exists.
        if not sname in self.sections:
            raise Exception("Section '%s' does not exists." % ( name ) )

        # Make sure the key is a string.
        if not isinstance(key, basestring):
            raise Exception("Section key must be a string.")

        # Convert value to a KIniValue object if necessary, and set the doc
        # attribute.
        if isinstance(value, KIniValue):
            if doc: value.doc = doc
            self.sections[sname][key] = value
        else:
            ini_value = KIniValue(value, doc)
            self.sections[sname][key] = ini_value

    # Write to a string.
    def write_to_string(self):
        s = "# WARNING: THIS FILE IS AUTO-GENERATED.\n\n"

        # Insert the file main comment, if any.
        if self.doc: s += self.format_comment(self.doc) + '\n\n'

        for name, section in self.sections.items():
            if section.doc: s += self.format_comment(section.doc)
            s += '[' + name + ']\n'
            for key, kinivalue in section.items():
                if kinivalue.doc: s += self.format_comment(kinivalue.doc)
                s += str(key) + '=' + str(kinivalue.value) + '\n\n'
            s += '\n'

        return s

