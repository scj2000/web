<?php
/**
 * @file
 * The primary PHP file for this theme.
 */

/**
 * Overrides video_filter theme function to make the video responsive.
 */
function scj2000net_video_filter_iframe($variables) {
  $video = $variables['video'];
  $classes = video_filter_get_classes($video);
  $classes[] = 'embed-responsive-item';
  $attributes['classes'] = $classes;
  $str_attributes = '';
  if (!empty($video['attributes'])) {
    $attributes = array_merge_recursive($video['attributes'], $attributes);
  }
  $str_attributes = drupal_attributes($attributes);

  $output =<<<EOF
<div class="video-filter-wrapper col-xs-12 col-sm-10 col-md-8 col-lg-8 padding-zero">
  <div class="video-filter embed-responsive embed-responsive-16by9">
    <iframe src="${video['source']}" width="${video['width']}" height="${video['height']}" frameborder="0" allowfullscreen="true" ${str_attributes}></iframe>
  </div>
</div>
EOF;
  return $output;
}
