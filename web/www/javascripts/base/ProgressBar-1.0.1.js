/************************************************************************************
 *  ProgressBar, version 1.0.1												*
 *  (c) 2008 Pierrick HYMBERT pierrick [ dot ] hymbert [ at ] gmail [ dot ] com							*
 *																					*
 *  ProgressBar is freely distributable                                           *
 *  and "PROVIDED AS IS" under the terms of an MIT-style license.	                *
 *																					*
/************************************************************************************/

if(( typeof Prototype == 'undefined' ))
      throw("ProgressBar requires the Prototype JavaScript framework >= 1.6");

/**
 * User interface object that is used to display progress in the form of a bar.
 * 
 * @class ProgressBar
 * @version 1.0.1
 * @author phymbert
 * @create 01/04/2007
 * @modified 27/07/2008
 */
ProgressBar = Class.create();

/**
 * The ProgressBar API version.
 * @type String
 */
ProgressBar.Version = '1.0.1'

/** 
 * Progress bar style indeterminate.
 * @type Integer
 */
ProgressBar.INDETERMINATE = 1;

/** 
 * Progress bar style determinate.
 * @type Integer
 */
ProgressBar.DETERMINATE = 1<<1;

/** 
 * Progress bar transparency style.
 * @type Integer
 */
ProgressBar.TRANSPARENCY = 1<<2;

/**
 * Class definition.
 */
ProgressBar.prototype =  {
	/**
	 * Constructs a new instance of this class given its container 
	 * and a options map describing its behavior and appearance. 
	 * 
	 * @constructor
	 * 
	 * @param container:Element The html element where the bar will be drawn in.
	 * 
	 * @param options.style:Integer
	 * 			Only one of the styles INDETERMINATE with/without TRANSPARENCY or DETERMINATE may be specified. 
	 * 
	 * @param options.color:Map
	 * 			Widget color.
	 * 
	 * @param options.colorEnd:Map
	 * 			If the receiver behavior is DETERMINATE, the color end specified which color the bar items have when selection == maximum.
	 * 
	 * @param options.classProgressBar:String
	 * 			The CSS class to defined position, top, left, width, height receiver attributes. 
	 * 
	 * @param options.minimum:Integer
	 * 			The selection minimum value (must be zero or greater). 
	 * 
	 * @param options.maximum:Integer
	 * 			The selection maximum value (must be zero or greater). 
	 * 
	 * @param options.selection:Integer
	 * 			The receiver current selection (must be zero or greater). 
	 * 
	 * @param options.executerInterval :Integer
	 * 			The interval (in milliseconds) for the update receiver function in the indeterminate behavior.
	 * 
	 * @param options.noIndeterminateIndicators :Integer
	 * 			The number of indicators for the indeterminate behavior.
	 * 
	 * @param options.widthIndicators :Integer
	 * 			The indicator width.
	 * 
	 * @param options.indeterminateIndicatorsSpeed :Integer
	 * 			The indicator moving speed in pixel by options.executerInterval.
	 * 
	 * @public
	 */
   initialize: function( container, options ) {
    
	    /**
	     * Default receiver options
	     */
	    this.options = 	{
	    				style: ProgressBar.INDETERMINATE,
	    				color: {r: 38, g: 255, b: 38},
	    				colorEnd: null,
	    				classProgressBar: 'progressBar',
	    				minimum: 0,
	    				maximum: 100,
	 					selection: 0,
	 					executerInterval: 50,
	 					noIndeterminateIndicators: 10,
	 					widthIndicators: 6,
	 					indeterminateIndicatorsSpeed: 5
	    			}
    	
    	// Set user options
   		Object.extend( this.options, options ? options : {} );
   		
   		// Set the progress bar children
   		this.progressIndicators = [];
   		
   		// This widget is currently not disposed
   		this.disposed = false;
   		
   		// User data
   		this.data = null;
   		
   		// User map data
   		this.datas = new Hash();
   		
   		// Initialize DHTML elements
   		this._drawDhtmlElements( container );
   		
   		// Set the progress bar values
   		this.setMaximum( this.options.maximum );
   		this.setMinimum( this.options.minimum );
   		this.setSelection( this.options.selection );
   		
   		
   		// If progress bar behavior is indeterminate,
   		// start update function.
   		if( ( this.options.style & ProgressBar.INDETERMINATE ) == ProgressBar.INDETERMINATE ){
   			this.lastTime = new Date();
			this.executer = new PeriodicalExecuter(this._updateIndeterminate.bind(this), this.options.executerInterval / 1000);
   		}
    },
    
    /**
	 * Returns the maximum value which the receiver will allow.
	 * @return Integer
	 * @public
	 */
    getMaximum: function(){ return this.maximum; },
    
	/**
	 * Returns the minimum value which the receiver will allow. 
     * @return Integer
	 * @public
	 */  
	getMinimum: function(){ return this.minimum; },  
	          
	/**
	 * Returns the single 'selection' that is the receiver's position. 
     * @return Integer
	 * @public
	 */  
	getSelection: function(){ return this.selection; }, 
	          
	/**
	 * Sets the maximum value that the receiver will allow to be the argument which must be greater than or equal to zero. 
     * @param value:Integer The new maximum (must be zero or greater)
	 * @public
	 */  
	setMaximum: function(value) { this.maximum = value; },
	          
	/**
	 * Sets the minimum value that the receiver will allow to be the argument which must be greater than or equal to zero.   
	 * @param value:Integer The new minimum (must be zero or greater)
	 * @public
	 */  
	setMinimum: function(value) { this.minimum = value; },
	          
	/**
	 * Sets the single 'selection' that is the receiver's position to the argument which must be greater than or equal to zero. 
	 * @param value:Integer The new selection (must be zero or greater)
	 * @public
	 */  
	setSelection: function(value) { 
		if( value >= this.minimum && value <= this.maximum) {
			this.selection = value;
			//Update the receiver
			this._updateSelection();
		}else{
			throw   "ProgressBar: Invalid argument exception occurs while setting receiver selection.";
		}
	},
	
	/**
	 * Returns true if the widget has been disposed, and false otherwise
	 * @return Boolean
	 * @public
	 */
	isDisposed: function(){
		return this.disposed;
	},
	
	/**
	 * Disposes the receiver and all its HTML Objects descendants.
	 * @public
	 */
	dispose: function(){
		if( !this.isDisposed() ){
			this.disposed = true;
			this.progressBar.remove();
		}
	},
	
	/**
	 * Returns the application defined property of the receiver with the specified name,
	 * or null if it has not been set.
	 * Applications may have associated arbitrary objects with the receiver in this fashion.
	 * 
	 * @param key:String the name of the property
	 * @return Object
	 */
	getData: function( key ){
		if( arguments.length == 0 ){
			return this.data;
		}else{
			return this.datas.get(key);
		}
	},
	
	/**
	 * Sets the application defined widget data associated with the receiver to be the argument.
	 * @param key:String the name of the property
	 * @param value:String the new value for the property 
	 */
	setData: function(key, value){
		if( arguments.length == 1 ){
			return this.data = key;
		}else{
			return this.datas.set(key, value);
		}
	},
	
    /**
	 * Return the class identifier.
	 * @return String
	 * @public
	 */
    toString: function(){
    	return "ProgressBar";
    },
    
	/**
	 * Callback function which increment the current selection.
	 * @param executer:PeriodicalExecuter The call back caller.
	 * @private
	 */
	_updateIndeterminate: function( executer ){
		if( this.isDisposed() ){
			executer.stop();
			return;
		}
		this.setSelection( ( this.selection + 1 ) % this.maximum );
	},
	
	/**
	 * Update the receiver selection indicators.
	 * @private
	 */
	_updateSelection: function(){
		if( ( this.options.style & ProgressBar.INDETERMINATE ) == ProgressBar.INDETERMINATE ){
			//Indeterminate behavior: update indicators position
			this._updateSelectionIndeterminate();
		}else{
			//Update the number of indicators in function of current selection
			var nbIndicatorsMax = Math.round( this.progressBarContent.getWidth() / this.options.widthIndicators );
			var nbIndicators = Math.round( nbIndicatorsMax * this.selection / ( this.maximum - this.minimum ) );
			
			//If the number of indicators is incremented
			var i = this.progressIndicators.length;
			while( this.progressIndicators.length < nbIndicators ){
				var element = this._createIndicator( ( i * ( this.options.widthIndicators + 1 ) ) + 'px' );
				this.progressIndicators[ this.progressIndicators.length ] = element;
				i++;
			}
			
			//If the number of indicators is decremented
			i = this.progressIndicators.length - 1;
			while( i >= 0 && this.progressIndicators.length > nbIndicators ){
				var element = this.progressIndicators[ i ];
				this.progressIndicators[ i ] = null;
				this.progressIndicators.length --;
				this.progressBarContent.removeChild( element );
				i--;
			}
			
			// Update the indicators color.
			this._updateIndicatorsColor();
		}
	},
	
	/**
	 * Update the indicators position
	 * @private
	 */
	_updateSelectionIndeterminate: function(){
		// Create progress bar indicators
		var i = 0;
		var r = parseInt( Math.random() * 10 );
		while( this.progressIndicators.length < this.options.noIndeterminateIndicators ){
			var element = this._createIndicator( );
			element.style.left = ( ( this.options.noIndeterminateIndicators - i + 1) * ( this.options.widthIndicators + 1 ) + r ) + 'px';
			
			if( ( this.options.style & ProgressBar.TRANSPARENCY ) == ProgressBar.TRANSPARENCY ){
				element.setOpacity( ( this.options.noIndeterminateIndicators - i ) / this.options.noIndeterminateIndicators );
			}
			
			this.progressIndicators[ this.progressIndicators.length ] = element;
			i++;
		}
		// Check the time past since last call
		var time = new Date();
		var delta = parseInt( ( time - this.lastTime ) / this.options.executerInterval ) + this.options.indeterminateIndicatorsSpeed;
		this.lastTime = time;
			
		// Move progress bar indicators
		for(var i = this.progressIndicators.length - 1; i >= 0 ; i--){	
			var element = this.progressIndicators[i];
			var left = ( parseInt( element.style.left ) + delta ) % ( element.parentNode.clientWidth );
			if( !isNaN( left ) )
				element.style.left = left + 'px';
		}
	},
	
	/**
	 * Update the indicators color
	 * @private
	 */
	_updateIndicatorsColor: function( color ){
		if( this.options.colorEnd == null ){
			return;
		}
		var percent = this.selection / (this.maximum - this.minimum );
		if( Object.isUndefined( this.lastPercent ) ){
			this.lastPercent = percent;
		}else if( percent > this.lastPercent && percent - this.lastPercent > 0.1 
				|| percent < this.lastPercent && this.lastPercent - percent  > 0.1
				){
			this.lastPercent = percent;
		}else{
			return;
		}
		
		//Construct the current color
		var currentColor = {
			r: parseInt( this.options.color.r < this.options.colorEnd.r ? 
				this.options.color.r + ( this.options.colorEnd.r - this.options.color.r ) * percent
				: this.options.color.r - ( this.options.color.r - this.options.colorEnd.r) * percent
				)
			,
			g: parseInt( this.options.color.g < this.options.colorEnd.g ? 
				this.options.color.g + ( this.options.colorEnd.g - this.options.color.g ) * percent
				: this.options.color.g - ( this.options.color.g - this.options.colorEnd.g) * percent
				)
			,
			b: parseInt( this.options.color.b < this.options.colorEnd.b ? 
				this.options.color.b + ( this.options.colorEnd.b - this.options.color.b ) * percent
				: this.options.color.b - ( this.options.color.b - this.options.colorEnd.b) * percent
				)
		};
		var colorString = 'rgb(' + currentColor.r + ',' + currentColor.g + ',' + currentColor.b + ')';
		
		//Update HTML element background color
		this.progressBarContent.childElements().each( function(i){
			i.childElements().each( function(e){
				e.setStyle({backgroundColor: colorString});
			})
		});
	},
	
	/**
	 * Draw an indicator.
	 * @private
	 */
	_createIndicator: function( left ){
			var element = this._createElement( ['absolute', null, left, this.options.widthIndicators + 'px', '100%', null, this.progressBarContent ]);
			
			var bgColor = 'rgb(' + this.options.color.r + ',' + this.options.color.g + ',' + this.options.color.b + ')';
			if( this.options.colorEnd != null && ( this.options.style & ProgressBar.DETERMINATE ) == ProgressBar.DETERMINATE){
				var percent = this.selection / (this.maximum - this.minimum );
				var currentColor = {
					r: parseInt( this.options.color.r < this.options.colorEnd.r ? 
						this.options.color.r + ( this.options.colorEnd.r - this.options.color.r ) * percent
						: this.options.color.r - ( this.options.color.r - this.options.colorEnd.r) * percent
						)
					,
					g: parseInt( this.options.color.g < this.options.colorEnd.g ? 
						this.options.color.g + ( this.options.colorEnd.g - this.options.color.g ) * percent
						: this.options.color.g - ( this.options.color.g - this.options.colorEnd.g) * percent
						)
					,
					b: parseInt( this.options.color.b < this.options.colorEnd.b ? 
						this.options.color.b + ( this.options.colorEnd.b - this.options.color.b ) * percent
						: this.options.color.b - ( this.options.color.b - this.options.colorEnd.b) * percent
						)
				};
				bgColor = 'rgb(' + currentColor.r + ',' + currentColor.g + ',' + currentColor.b + ')';
			}
			var height = parseInt( this.progressBarContent.clientHeight / 6 );
			var filtersParams = [
				['absolute', '0px', null, '100%', height + 'px', bgColor, element, 0.4],
				['absolute', height + 'px', null, '100%', height + 'px', bgColor, element, 0.5],
				['absolute', ( 2 * height ) + 'px', null, '100%', height + 'px', bgColor, element, 0.6],
				['absolute', ( 3 * height ) + 'px', null, '100%', height + 'px', bgColor, element, 0.7],
				['absolute', ( 4 * height ) + 'px', null, '100%', height + 'px', bgColor, element, 0.7],
				['absolute', ( 5 * height ) + 'px', null, '100%', height + 'px', bgColor, element, 0.6],
				['absolute', ( 6 * height ) + 'px', null, '100%', height + 'px', bgColor, element, 0.5]
			];
			this._createElements(filtersParams);		
			return element;
	},
	
 	/**
 	 * Draw the progress bar.
	 * @private
 	 */
    _drawDhtmlElements: function( container ){
   		this.progressBar = new Element('div', {'class' : this.options.classProgressBar});
		$(container).insert( this.progressBar );
		
		this.progressBarContent = this._createElement(['absolute', '2px', '3px', (this.progressBar.clientWidth - 6) + 'px', (this.progressBar.clientHeight - 6) + 'px', null, this.progressBar]);
		this.progressBarContent.style.overflow = 'hidden';
		
		var borderParams = [
			// Border top 1
			['absolute', '0px', '1px', '1px', '1px', 'rgb(172, 171, 167)', this.progressBar],
			['absolute', '0px', '2px', '1px', '1px', 'rgb(127, 126, 125)', this.progressBar],
			['absolute', '0px', '3px', (this.progressBar.clientWidth - 5) + 'px', '1px', 'rgb(104, 104, 104)', this.progressBar],
			// Border top 2
			['absolute', '1px', '0px', '1px', '1px', 'rgb(172, 171, 167)', this.progressBar],
			['absolute', '1px', '1px', '1px', '1px', 'rgb(119, 119, 119)', this.progressBar],
			['absolute', '1px', '2px', (this.progressBar.clientWidth - 3) + 'px', '1px', 'rgb(190, 190, 190)', this.progressBar],
			['absolute', '1px', (this.progressBar.clientWidth - 3) + 'px', '1px', '1px', 'rgb(104, 104, 104)', this.progressBar],
			['absolute', '1px', (this.progressBar.clientWidth - 2) + 'px', '1px', '1px', 'rgb(179, 179, 179)', this.progressBar],
			// Border top 3
			['absolute', '2px', '0px', '1px', '1px', 'rgb(127, 126, 125)', this.progressBar],
			['absolute', '2px', '1px', '2px', '1px', 'rgb(190, 190, 190)', this.progressBar],
			['absolute', '2px', '3px', (this.progressBar.clientWidth - 6) + 'px', '1px', 'rgb(239, 239, 239)', this.progressBar],
			['absolute', '2px', (this.progressBar.clientWidth - 3) + 'px', '1px', '1px', 'rgb(190, 190, 190)', this.progressBar],
			['absolute', '2px', (this.progressBar.clientWidth - 2) + 'px', '1px', '1px', 'rgb(129, 129, 129)', this.progressBar],
			// Border left
			['absolute', '3px', '0px', '1px', (this.progressBar.clientHeight - 6) + 'px', 'rgb(104, 104, 104)', this.progressBar],
			['absolute', '3px', '1px', '1px', (this.progressBar.clientHeight - 6) + 'px', 'rgb(190, 190, 190)', this.progressBar],
			// Border right
			['absolute', '3px', (this.progressBar.clientWidth - 1) + 'px', '1px', (this.progressBar.clientHeight - 6) + 'px', 'rgb(104, 104, 104)', this.progressBar],
			['absolute', '3px', (this.progressBar.clientWidth - 2) + 'px', '1px', (this.progressBar.clientHeight - 6) + 'px', 'rgb(190, 190, 190)', this.progressBar],
			// Border bottom 1
			['absolute', (this.progressBar.clientHeight - 1) + 'px', '1px', '1px', '1px', 'rgb(239, 239, 239)', this.progressBar],
			['absolute', (this.progressBar.clientHeight - 1) + 'px', '2px', '1px', '1px', 'rgb(127, 126, 125)', this.progressBar],
			['absolute', (this.progressBar.clientHeight - 1) + 'px', '3px', (this.progressBar.clientWidth - 5) + 'px', '1px', 'rgb(104, 104, 104)', this.progressBar],
			// Border bottom 2
			['absolute', (this.progressBar.clientHeight - 2) + 'px', '0px', '1px', '1px', 'rgb(172, 171, 166)', this.progressBar],
			['absolute', (this.progressBar.clientHeight - 2) + 'px', '1px', '1px', '1px', 'rgb(119, 119, 119)', this.progressBar],
			['absolute', (this.progressBar.clientHeight - 2) + 'px', '2px', (this.progressBar.clientWidth - 3) + 'px', '1px', 'rgb(190, 190, 190)', this.progressBar],
			['absolute', (this.progressBar.clientHeight - 2) + 'px', (this.progressBar.clientWidth - 3) + 'px', '1px', '1px', 'rgb(104, 104, 104)', this.progressBar],
			['absolute', (this.progressBar.clientHeight - 2) + 'px', (this.progressBar.clientWidth - 2) + 'px', '1px', '1px', 'rgb(179, 179, 179)', this.progressBar],
			// Border bottom 3
			['absolute', (this.progressBar.clientHeight - 3) + 'px', '0px', '1px', '1px', 'rgb(127, 126, 125)', this.progressBar],
			['absolute', (this.progressBar.clientHeight - 3) + 'px', '1px', '2px', '1px', 'rgb(190, 190, 190)', this.progressBar],
			['absolute', (this.progressBar.clientHeight - 3) + 'px', '3px', (this.progressBar.clientWidth - 6) + 'px', '1px', 'rgb(239, 239, 239)', this.progressBar],
			['absolute', (this.progressBar.clientHeight - 3) + 'px', (this.progressBar.clientWidth - 3) + 'px', '1px', '1px', 'rgb(190, 190, 190)', this.progressBar],
			['absolute', (this.progressBar.clientHeight - 3) + 'px', (this.progressBar.clientWidth - 2) + 'px', '1px', '1px', 'rgb(129, 129, 129)', this.progressBar]
		];
		this._createElements(borderParams);		
	},
	
	/**
	 * Create elements defined in the map.
	 * @private
	 */
	_createElements: function( elementsParams ){
		elementsParams.each( function( e ){
				this._createElement(e);
			},
			this
		);
	},
	
	/**
	 * Create an HTML DIV element.
	 * @param elementParams[0]:String position
	 * @param elementParams[1]:String top
	 * @param elementParams[2]:String left
	 * @param elementParams[3]:String width
	 * @param elementParams[4]:String height
	 * @param elementParams[5]:String backgroundColor
	 * @param elementParams[6]:String parentNode
	 * @private
	 */
	_createElement: function( elementParams ){
		var element = new Element('div');
		var style = element.style;
		if( elementParams[0] != null )
			style.position = elementParams[0];
			
		if( elementParams[1] != null )
			style.top = elementParams[1];
			
		if( elementParams[2] != null )
			style.left = elementParams[2];
			
		if( elementParams[3] != null )
			style.width = elementParams[3];
			
		if( elementParams[4] != null )
			style.height = elementParams[4];
			
		if( elementParams[5] != null )
			style.backgroundColor = elementParams[5];
			
		if( elementParams[6] != null )
			$(elementParams[6]).insert( element );
			
		if( elementParams[7] != null )
			element.setOpacity( elementParams[7] );
		style.fontSize = '0px';
		return element;
	}
 };