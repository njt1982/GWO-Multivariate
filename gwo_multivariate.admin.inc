<?php

/**
 * Overview page
 */
function gwo_multivariate_admin_overview() {
  // Grab the experiments list
  $experiments = gwo_multivariate_build_experiments();

  // Create a table summarising the experiments
  $rows = array();
  foreach ($experiments as $e) {
    $rows[] = array(
      $e->experimentId,
      $e->title,
      array('class' => 'status-'. drupal_strtolower($e->status), 'data' => $e->status), 
      $e->numSections,
      l(t('Edit'), 'admin/settings/gwo-multivariate/edit/'. $e->experimentId) .' | '. l(t('Delete'), 'admin/settings/gwo-multivariate/delete/'. $e->experimentId),
    );
  }

  $headers = array(t('Experiment ID'), t('Title'), t('Status'), t('Sections'), t('Ops'));
  return drupal_get_form('gwo_multivariate_admin_refresh_form') . theme('table', $headers, $rows);
}


function gwo_multivariate_admin_refresh_form() {
  $form = array();
  $form['submit'] = array('#type' => 'submit', '#value' => t('Refresh'));
  return $form;
}

function gwo_multivariate_admin_refresh_form_submit($form, &$form_state) {
  cache_clear_all('gwo_multivariate', 'cache');
}

/**
 * Create/Edit experiment form
 */
function gwo_multivariate_admin_experiment_form(&$form_state, $experiment = NULL) {
  $form = array();
  $front = url('<front>', array('absolute' => TRUE));

  $testUrl = $goalUrl = '';

  if ($experiment) {
    $form_state['experiment'] = $experiment;
  }

  $form['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Experiment Title'),
    '#description' => t('The experiment title should be used to uniquely identify this experiment'),
    '#required' => TRUE,
    '#default_value' => isset($experiment) ? $experiment->title : '',
  );

  $form['testUrl'] = array(
    '#type' => 'textfield',
    '#title' => t('Test Url'),
    '#description' => t('Enter the test page url. It will probably start with @url', array('@url' => $front)),
    '#required' => TRUE,
    '#default_value' => isset($experiment) ? $experiment->link['gwo:testUrl'] : '',
  );

  $form['goalUrl'] = array(
    '#type' => 'textfield',
    '#title' => t('Conversion Url'),
    '#description' => t('Enter the conversion page url. It will probably start with @urli. This may also be referred to as a Goal URL.', array('@url' => $front)),
    '#required' => TRUE,
    '#default_value' => isset($experiment) ? $experiment->link['gwo:goalUrl'] : '',
  );

  // We can only add variables to existing experiments
  // TODO: Find a work around for this...
  if ($experiment) {
    $form['variables'] = array(
      '#type' => 'fieldset',
      '#title' => t('Variables'),
      '#tree' => TRUE,
    );
    $section_titles = array();
    foreach ($experiment->sections as $s) {
      list($module) = explode('-', $s->title);
      $section_titles[$module][$s->title] = $s->title;
    }
   
    $variables = module_invoke_all('gwo_variables');
    foreach ($variables as $module => $variable_settings) {
      $form['variables'][$module] = array(
        '#type' => 'fieldset',
        '#title' => $variable_settings['name'],
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
      );
   
      $form['variables'][$module][$module] = array(
        '#type' => 'checkboxes',
        '#title' => t('Enabled Blocks'),
        '#options' => $variable_settings['options'],
        '#default_value' => isset($section_titles[$module]) ? $section_titles[$module] : array(),
        '#parents' => array('variables', $module),
      );
    }
  }

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );

  return $form;
}


function block_gwo_variables() {
  $blocks = _block_rehash();
  $options = array();
  foreach ($blocks as $block) {
    $options[$block['module'] .'-'. $block['delta']] = t('%info (%module - %delta)', array('%info' => $block['info'], '%module' => $block['module'], '%delta' => $block['delta']));
  }

  return array('block' => array(
    'name' => t('Blocks'),
    'options' => $options,
  ));
} 


function content_gwo_variables() {
  $options = array();
  foreach (content_fields() as $field_id => $field) {
    $options[$field_id] = t('%name (%id)', array('%name' => $field['widget']['label'], '%id' => $field_id));
  }

  return array('content' => array(
    'name' => t('CCK Fields'),
    'options' => $options,
  ));
}


function gwo_multivariate_admin_experiment_form_submit($form, &$form_state) {
  $args = array(
    'url' => GOOGLE_API_URL,
    'method' => 'POST',
    'request_type' => 'xml',
  );
  $exp = array(
    'title' => $form_state['values']['title'],
    'testUrl' => $form_state['values']['testUrl'],
    'goalUrl' => $form_state['values']['goalUrl'],
  );

  if ($form_state['experiment']) {
    $id = (int)$form_state['experiment']->experimentId; 
    $args['url'] = GOOGLE_API_URL ."/{$id}";
    $args['method'] = 'PUT';
    $exp['etag'] = $args['etag'] = $form_state['experiment']->etag;
  }
  $args['data'] = _gwo_multivariate_create_experiment($exp);

  $res = oauth_google_request($args);

  if (isset($form_state['values']['variables'])) {
    $form_state['values']['variables'] = array_map('array_filter', $form_state['values']['variables']);
    foreach($form_state['values']['variables'] as $sections) {
      foreach($sections as $section) {
        $s_args = array(
          'url' => GOOGLE_API_URL ."/{$id}/sections",
          'method' => 'POST',
          'request_type' => 'xml',
          'data' => _gwo_multivariate_create_experiment_section(array('title' => $section)),
        );
        $r = oauth_google_request($s_args);
      }
    }
  }

  cache_clear_all('gwo_multivariate', 'cache');
  $form_state['redirect'] = 'admin/settings/gwo-multivariate';
}


/**
 * Delete confirm form to remove an experiment
 */
function gwo_multivariate_admin_delete_confirm(&$form_state, $experiment) {
  $form = array();

  $form_state['experiment'] = $experiment;
  return confirm_form(
    $form,
    t('Are you sure you want to delete the test "%name"?', array('%name' => $experiment->entry->title->{'$t'})),
    'admin/settings/gwo-multivariate',
    '<strong>'. t('Important: once deleted, this cannot be retreived') .'</strong>',
    t('Delete'),
    t('Cancel'),
    'gwo_multivariate_admin_delete_confirm'
  );
}


/**
 * Submit handler for the delete experiment handler
 */
function gwo_multivariate_admin_delete_confirm_submit(&$form, &$form_state) {
  // Convert the ID from a "url" into a number.
  $id = $form_state['experiment']->entry->{'gwo$experimentId'}->{'$t'};

  // Issue a delete request
  $res = oauth_google_request(array('url' => GOOGLE_API_URL ."/{$id}", 'method' => 'DELETE', 'request_type' => 'xml'));

  // If we get something other than a 200 code back, there was a problem
  if ($res->code != 200) {
    drupal_set_message(t('Error deleting experiment. %code : %msg', array('%code' => $res->code, '%msg' => $res->status_message)), 'error');
  }

  // Send the user back to the overview page
  $form_state['redirect'] = 'admin/settings/gwo-multivariate';
}


/**
 * General purpose settings form
 */
function gwo_multivariate_admin_settings_form() {
  $form = array();

  $form['gwo_multivariate_account_id'] = array(
    '#type' => 'textfield',
    '#title' => t('Google Account ID'),
    '#description' => t('A numeric account ID'),
    '#required' => TRUE,
    '#default_value' => variable_get('gwo_multivariate_account_id', ''),
  );

  return system_settings_form($form);
}


/**
 * Create/Update Experiment wrapper function
 */
function _gwo_multivariate_create_experiment($args) {
  $args += array(
    'account_id' => variable_get('gwo_multivariate_account_id', ''),
  );
  if (isset($args['etag'])) {
    $args['etag'] = ' gd:etag="'. htmlspecialchars($args['etag']) .'"';
  }

  $data = <<<EOT
<entry xmlns='http://www.w3.org/2005/Atom'
       xmlns:gwo='http://schemas.google.com/analytics/websiteoptimizer/2009'
       xmlns:app='http://www.w3.org/2007/app'
       xmlns:gd='http://schemas.google.com/g/2005'{$args['etag']}>
  <title>{$args['title']}</title>
  <gwo:analyticsAccountId>{$args['account_id']}</gwo:analyticsAccountId>
  <gwo:experimentType>Multivariate</gwo:experimentType>
  <link rel='gwo:testUrl' type='text/html' href='{$args['testUrl']}' />
  <link rel='gwo:goalUrl' type='text/html' href='{$args['goalUrl']}' />
</entry>
EOT;
  return $data;
}


/**
 * Create/Update Experiment Section wrapper function
 */
function _gwo_multivariate_create_experiment_section($args) {
  if (isset($args['etag'])) {
    $args['etag'] = ' gd:etag="'. htmlspecialchars($args['etag']) .'"';
  }

  $data = <<<EOT
<entry xmlns='http://www.w3.org/2005/Atom'
       xmlns:gwo='http://schemas.google.com/analytics/websiteoptimizer/2009'
       xmlns:app='http://www.w3.org/2007/app'
       xmlns:gd='http://schemas.google.com/g/2005'{$args['etag']}>
  <title>{$args['title']}</title>
</entry>
EOT;
  return $data;
}
