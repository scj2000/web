<?php

/**
 * @file
 * Contains the main purging functionality and error handling
 */

/**
 * Purges urls from CloudFlare cache.
 *
 * @param array $cfpurge_urls
 *   Array of urls to purge from CloudFlare cache using secure POST method.
 *
 * @return array
 *   Array of urls and their http status codes after purging.
 */
function cfpurge_urls($cfpurge_urls) {
  static $url_cache;
  $urls = array();

  // Initialize static URL array, used for preventing duplicate purges.
  if (!isset($url_cache)) {
    $url_cache = array();
  }

  // Attach the obove info to each URL.
  foreach ($cfpurge_urls as $url) {
    // Skips URLs which have already been purged.
    if (in_array($url, $url_cache)) {
      continue;
    }

    // Append absolute URL to data array.
    $absolute = url($url, array('alias' => 1, 'absolute' => 1));
    $prefixed = variable_get('cfpurge_url_prefix', url('', array('absolute' => TRUE))) .
      str_replace(url('', array('absolute' => TRUE)), '', $url);

    // Make sure page is cleared from local cache if enabled.
    if (variable_get('cache', 0)) {
      cache_clear_all($absolute, 'cache_page');
    }

    // Save the URL to a static variable.
    $url_cache[] = $url;

    // Append the URL to the purge queue.
    $urls[] = $prefixed;
  }

  // Get rid of blocks' content from Drupal's cache.
  if (count($urls) && module_exists('block') && variable_get('block_cache', 0)) {
    cache_clear_all('*', 'cache_block', 1);
  }

  return cfpurge_serial_request($urls);
}

/**
 * Issue cfpurge request using serial curl requests.
 *
 * @param array $urls
 *   Array of purge requests ready to be sent using curl.
 *
 * @return array
 *   Array of purge requests along with their responses after purging.
 */
function cfpurge_serial_request($urls) {
  // If no urls don't run.
  if (empty($urls)) {
    return $urls;
  }

  // Make sure the purge URL exists.
  if (!$purge_url = variable_get('cfpurge_purge_url', FALSE)) {
    watchdog('cfpurge', 'Your purge URL is not defined. Please save your Cloudflare Purge Settings configuration to generate this zones purge url.', array(), WATCHDOG_ERROR);
    return $urls;
  }

  $data['files'] = $urls;
  $curl_cfpurge = curl_init();
  curl_setopt($curl_cfpurge, CURLOPT_URL, $purge_url);
  curl_setopt($curl_cfpurge, CURLOPT_RETURNTRANSFER, 1);
  curl_setopt($curl_cfpurge, CURLOPT_CUSTOMREQUEST, 'DELETE');
  curl_setopt($curl_cfpurge, CURLOPT_POSTFIELDS, drupal_json_encode($data));
  curl_setopt($curl_cfpurge, CURLOPT_TIMEOUT, 5);
  curl_setopt($curl_cfpurge, CURLOPT_FOLLOWLOCATION, TRUE);
  curl_setopt($curl_cfpurge, CURLOPT_AUTOREFERER, TRUE);
  curl_setopt($curl_cfpurge, CURLOPT_SSL_VERIFYPEER, FALSE);
  curl_setopt($curl_cfpurge, CURLOPT_HTTPHEADER, array(
    'User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0) Gecko/20100101 Firefox/10.0',
    'X-Auth-Email: ' . variable_get('cfpurge_email'),
    'X-Auth-Key: ' . variable_get('cfpurge_api_key'),
    'Content-Type: application/json',
  ));

  $response = curl_exec($curl_cfpurge);
  $body_array = explode("\r\n\r\n", $response);
  $body = array_pop($body_array);

  $info = curl_getinfo($curl_cfpurge);
  $data['http_code'] = $info['http_code'];
  $data['content'] = drupal_json_decode($body);

  return $data;
}

/**
 * Logs successful CloudFlare purges and errors to the watchdog.
 *
 * @param array $responses
 *   Array of purge requests along with their responses.
 */
function cfpurge_logging($response) {
  $purges = $errors = array();
  if (!empty($response['http_code'])) {
    if ($response['http_code'] == 200 && $response['content']['success'] == '1') {
      $purges = $response['files'];
    }
    elseif ($response['http_code'] == 200 && $response['content']['success'] != '1') {
      foreach ($response['content']['errors'] as $error) {
        $errors[] = $error;
      }
    }
    else {
      $errors[] = 'Error, There was a problem connecting to CloudFlare, here\'s the HTTP error code: ' . $response['http_code'];
    }
  }
  elseif (!empty($response) && empty($response['http_code'])) {
    // $response is an array of URLs at this point.
    $errors = $response;
  }
  else {
    watchdog('cfpurge', 'There was a problem connecting to CloudFlare. Please check your site configuration.', array(), WATCHDOG_ERROR);
  }

  // Log number of succesful purges along with the URLs.
  if (count($purges)) {
    watchdog(
      'cfpurge',
      '!success_count URLs have been successfully purged from CloudFlare: !purge_log',
      array(
        '!success_count' => count($purges),
        '!purge_log' => implode(', ', $purges),
      )
    );
  }

  // Log number of errors encountered along with the URLs.
  if (count($errors)) {
    watchdog(
        'cfpurge',
        '!errors_count errors have been encountered when purging these URLs: !cfpurge_log',
        array(
          '!errors_count' => count($errors),
          '!cfpurge_log' => implode(', ', $errors),
        ),
        WATCHDOG_ERROR
      );
  }
}
