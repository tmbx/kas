// This function splits a string with an array of regular expression strings.
// Text parts that match regular expressions are part of the returned data.
// Returns an array whith contains two arrays:
// - First sub-array contains an ordered list of items data: regular text parts or regexp match parts.
// - Second sub-array contains the item type:
//   - null: regular text part
//   - integer: index of the matching regular expression
function string_split_regexp_arr(s, regexp_arr)
{
    var i, cur_s;
    var tmp_item, tmp_item_len, tmp_start, tmp_stop;
    var item, item_len, start, stop, regexp_idx;
    var tmp_arr;
    var arr_data = [];
    var arr_type = [];

    cur_s = s;
    while (1)
    {
        start = 9999999999; // FIXME
        for (i=0; i<regexp_arr.length; i++)
        {
            // Find the first match in the remaining string.
            re = new RegExp(regexp_arr[i], "g");
            tmp_arr = re.exec(cur_s);
            if (tmp_arr)
            {
                tmp_item = tmp_arr[0];
                tmp_item_len = tmp_item.length;
                tmp_start = re.lastIndex - tmp_item_len;
                tmp_stop = re.lastIndex;
                if (tmp_start < start)
                {
                    // This is the first match.
                    item = tmp_item;
                    item_len = tmp_item_len;
                    start = tmp_start;
                    stop = tmp_stop;
                    regexp_idx = i;
                }
            }
        }
        if (start < 9999999999) // FIXME
        {
            // We found a match.
            if (start > 0)
            {
                // Extract the regular text part that precedes the regexp match from the remaining text.
                arr_data[arr_data.length] = cur_s.substring(0, start);
                arr_type[arr_type.length] = null;
            }
            if (start < stop) 
            {
                // Extract the regexp match from the remaining text.
                arr_data[arr_data.length] = cur_s.substring(start, stop);
                arr_type[arr_type.length] = regexp_idx;
            }
            // Set the _new_ remaining text.
            cur_s = cur_s.substring(stop, cur_s.length);
        }
        else
        {
            // No match... extract the remaining regular text and exit.
            arr_data[arr_data.length] = cur_s;
            arr_type[arr_type.length] = null;
            break;
        }
    }

    return [arr_data, arr_type];
}

// This function returns a cut string.
//  - max_len: maximum len which triggers the cut.
//  - cut_len: the length at which we cut the string, if max_len is reached.
//  - append_str: when cutting, append this string at the end of the cut string.
//  - options (binary field):
//    - STR_CUT_HANDLE_ENTITIES: do not cut within an HTML entity.
STR_CUT_HANDLE_ENTITIES = 1<<0;
function string_cut(s, max_len, cut_len, append_str, opts)
{
    var regexp_arr = [ "&#[0-9][0-9]?[0-9]?;", "&[a-zA-Z]+;" ];
    var i, arr, types, data, tmp_str, new_str;

    if (cut_len == undefined || cut_len > max_len) { cut_len = max_len; }
    if (append_str == undefined) { append_str = ''; }
    if (opts == undefined) { opts = 0; }

    new_str = '';
    if (s.length > max_len)
    {
        // Split string using html entity regular expressions.
        arr = string_split_regexp_arr(s, regexp_arr)
        data = arr[0];
        types = arr[1];
        i = 0;
        while (1)
        {
            // Append text part to the generated string.
            tmp_str = new_str + data[i];

            if (tmp_str.length > cut_len)
            {
                // New string is too big.

                if (types[i] == null || !(opts & STR_CUT_HANDLE_ENTITIES))
                {
                    // Item is a regular text part, or entity handling is not enabled.

                    // Cut the string at the specified length.
                    new_str = tmp_str.substring(0, cut_len);
                }
                else
                {
                    // Can't cut an item matching one of the regexps... item will be shorter than cut_len.
                }

                // Append specified string.
                new_str += append_str;

                break;
            }
            else
            {
                new_str = tmp_str;
            }

            i++;
        }

        return new_str;
    }
    else
    {
        return s;
    }
}

/*
    s = "begintest &#192;iii&#192; &#192; test 222 &eacute; &eacute; &eacute; test 333 &#192; &#192; &#192; endoftest";
    for (var i=0; i<s.length; i++)
    {
        ns = str_cut(s, 20, i, "...", STR_CUT_HANDLE_ENTITIES);
        document.write("NEW LEN: " + (ns.length-3) + "   NEW STRING: '" + ns + "'.<br>");
    }
*/


