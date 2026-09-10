(function ($) {
	"use strict";

    var $window = $(window),
            $body = $('body');

    /*======== Preloader ===========*/
    /* Reveal the page before anything else runs. This used to sit at the end
       of the ready handler, behind the plugin setup below -- so if any one of
       those threw, the curtain never lifted and the site was stuck on the
       spinner. It fires on DOM ready (fast, does not wait on images) and again
       on window load as a backstop. */
    function revealPage() {
        $body.addClass('loaded');
    }

    jQuery(document).ready(function($){

        revealPage();

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
        if ($.fn.scrollspy) {
            $body.scrollspy({
                target : ".navbar-collapse",
                offset : 95
            });
        }

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

        /*======== Modal js -- reload iframes on close so video stops ===========*/
        $('.modal').on('hidden.bs.modal', function () {
            $(this).find('iframe').each(function () {
                $(this).attr('src', $(this).attr('src'));
            });
        });

        /* Third-party plugins are guarded: a missing or failing one should
           degrade that single feature, not stop everything after it. */

        /*======== Magnific Popup ===========*/
        if ($.fn.magnificPopup) {
            $('.work-popup').magnificPopup({type:'image'});
        }

         /*========  CounterUp ===========*/
        if ($.fn.counterUp) {
            $('.counter').counterUp({
                delay: 4,
                 time: 800
            });
        }

        /*========   WOW js===========*/
        if (window.WOW) {
            new WOW({ mobile: false }).init();
        }

    });

    /* Backstop: even if the ready handler above failed outright, the page is
       revealed once everything has loaded. */
    $window.on('load', revealPage);

}(jQuery));
