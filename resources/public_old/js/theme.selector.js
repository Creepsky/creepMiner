(function($)
	{
		//  Stylesheets Selector
		var $links = $('link[rel*=alternate][title]');
		
		var el = document.getElementById('themeSelector'),
		elChild = document.createElement("div");
		elChild.innerHTML = '<select id="css-selector" class="selectpicker form-control" style="margin:0px;padding:0px"></select>';
		
		var options= '<option value="">cerulean</option>';
		$links.each(function(index,value){
			options +='<option value="'+$(this).attr('href')+'">'+$(this).attr('title')+'</option>';
		});
		$links.remove();
		
		el.appendChild(elChild);

		$('#css-selector').append(options)
			.bind('change',function(){
			$('link[rel*=jquery]').remove();
			$('head').append('<link rel="stylesheet jquery" href="'+$(this).val()+'" type="text/css" />');
		});
		
	}
)(jQuery);
