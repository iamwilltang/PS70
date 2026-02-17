(function ($) {
	"use strict";
    
    var $window = $(window),
            $body = $('body');
    
    jQuery(document).ready(function($){

        /*======== Sticky header ===========*/
        $('.navbar-collapse a').on('click',function(){
          $(".navbar-collapse").collapse('hide');
        });

        $window.on('scroll', function() {
          if ($(".navbar").offset().top > 100) {
            $(".navbar-fixed-top").addClass("top-nav-collapse");
              } else {
                $(".navbar-fixed-top").removeClass("top-nav-collapse");
              }
        });
      
        /*======== Smoothscroll js ===========*/
        $(function() {
          $('a.smooth-scroll').on('click', function(event) {
            var $anchor = $(this);
            $('html, body').stop().animate({
                scrollTop: $($anchor.attr('href')).offset().top - 0
            }, 1000);
            event.preventDefault();
          });
        });  
            
        /*======== jquery scroll spy ===========*/
        $body.scrollspy({
            target : ".navbar-collapse",
            offset : 95
        });
        
        /*========  Bootstrap menu fix ===========*/
        $(".navbar-toggle").on("click", function(){
            $body.addClass("mobile-menu-activated");
        });
        
        $("ul.nav.navbar-nav li a").on("click", function(){
            $(".navbar-collapse").removeClass("in");
        });
        
        /*======== background-image flickering solution for mobile ===========*/
        var bg = jQuery("#home");
        function resizeBackground() {
            bg.height(jQuery(window).height() + 60);
        }
      resizeBackground();
        
        /*======== Modal js ===========*/
        $("#projectModal1, #projectModal2, #projectModal3, #projectModal4, #projectModal5, #projectModal6, #projectModal7, #projectModal8, #projectModal9").on('hidden.bs.modal', function (e) {
            $("#projectModal1 iframe, #projectModal2 iframe, #projectModal3 iframe, #projectModal3 iframe, #projectModal4 iframe, #projectModal5 iframe, #projectModal6 iframe, #projectModal7 iframe, #projectModal8 iframe, #projectModal9 iframe").attr("src", $("projectModal1 iframe, #projectModal2 iframe, #projectModal3 iframe, #projectModal3 iframe, #projectModal4 iframe, #projectModal5 iframe, #projectModal6 iframe, #projectModal7 iframe, #projectModal8 iframe, #projectModal9 iframe").attr("src"));
        });
   
        /*======== Intro typer ===========*/
        var element = $(".element");
        $(function() {
            element.typed({
                strings: ["Carpe Diem.", "Ad Astra Per Aspera.", "Ad Meliora."],
                typeSpeed: 100,
                loop: true,
            });
        });
        
        /*======== Parallax js ===========*/
        $window.stellar({
            responsive: true,
            positionProperty: 'position',
            horizontalScrolling: false
        });
        
        /*======== Magnific Popup ===========*/
        $('.work-popup').magnificPopup({type:'image'});
        
         /*========  CounterUp ===========*/
        $('.counter').counterUp({
            delay: 4,
             time: 800
        });
 
        /*========   WOW js===========*/
        new WOW({ mobile: false }).init();
    
        /*======== jQuery mixItUp ===========*/
        $('.work-inner').mixItUp();
    
    

    });
    
   
    $window.on('load', function(){
        
        /*======== Preloader ===========*/
       setTimeout(function() {
            $('body').addClass('loaded');
        }, 2500);

    });

}(jQuery));	

