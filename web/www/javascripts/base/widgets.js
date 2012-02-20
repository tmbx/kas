// Box widget.
// Unused, might not work as-is.
var WIDGET_BOX_CLOSE_BUTTON = 1<<0;
var WIDGET_BOX_DEFAULT = WIDGET_BOX_CLOSE_BUTTON;

function widgetBox(e_id, opts)
{
    // Defaults
    if (opts == null) { opts = WIDGET_BOX_DEFAULT; }

    // this
    var _this = this;

    // Remove everything from container.
    this._clean = function() { this._o_container.innerHTML = ""; }

    // Show / hide box.
    this.show = function() { this._o_container.show(); }
    this.hide = function() { this._o_container.hide(); }

    // Close box.
    this.close = function() { this.hide(); this._clean(); }

    // Change the content of the box.
    this.setContent = function(content) { this._o_content.innerHTML = content; }

    // Element ID property
    this.id = e_id;

    // DOM Object properties
    this._o_container = null; // extended with prototype
    this._o_close_container = null;
    this._o_close_button = null;
    this._o_close_link = null;
    this._o_content_container = null;
    this._o_content = null;

    // Get the element object using jquery.
    this._o_container = get_by_id(e_id);
    if (!this._o_container) { throw("Invalid element ID."); }
    Element.extend(e_id);

    // Hide and clean element.
    this.hide();
    this._clean(); 

    // Customize element
    this._o_container.className = "ww-container";

    if (this.opts | WIDGET_BOX_CLOSE_BUTTON)
    {
        // Close button
        this._o_close_container = document.createElement("div");
        this._o_close_container.className = "ww-close-container";
        this._o_container.appendChild(this._o_close_container);
        this._o_close_button = document.createElement("div")
        this._o_close_button.className = "ww-close-button"; 
        this._o_close_container.appendChild(this._o_close_button);
        this._o_close_link = document.createElement("a");
        this._o_close_link.className = "ww-close-link";
        this._o_close_link.href = "#";
        this._o_close_link.innerHTML = "X";
        this._o_close_button.appendChild(this._o_close_link);
        $("#" + this.id + " .ww-close-link").bind("click", function() { _this.close(); });
    }

    // Content
    this._o_content_container = document.createElement("div");
    this._o_content_container.className = "ww-content-container";
    this._o_container.appendChild(this._o_content_container);
    this._o_content = document.createElement("div")
    this._o_content.className = "ww-content"; 
    this._o_content_container.appendChild(this._o_content);
}

