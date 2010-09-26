<?php

/**
 * Overview page
 */
function gwo_multivariate_admin_overview() {
  // Pull an experiment summary from the API
  $res = oauth_google_request();

  // If the code isn't 200 (OK) then output an error
  if ($res->code != 200) {
    drupal_set_message(t('Error getting experiment list from Google. %code: %msg', array('%code' => $res->code, '%msg' => $res->status_message)), 'error');
    return '';
  }

  // Convert the data from XML to an object, using SimpleXML
  $obj = json_decode($res->data);

  // Create a table summarising the experiments
  $rows = array();
  foreach ($obj->feed->entry as $experiment) {
    $id = $experiment->{'gwo$experimentId'}->{'$t'};

    $rows[] = array(
      check_plain($id),
      check_plain($experiment->title->{'$t'}),
      l(t('Edit'), 'admin/settings/gwo-multivariate/edit/'. $id) .' | '. l(t('Delete'), 'admin/settings/gwo-multivariate/delete/'. $id),
    );
  }

  $headers = array(t('Experiment ID'), t('Title'), t('Ops'));
  return theme('table', $headers, $rows);
}


function gwo_multivariate_admin_experiment_form(&$form_state, $experiment = NULL) {
  $form = array();
  $front = url('<front>', array('absolute' => TRUE));

  $testUrl = $goalUrl = '';

  if ($experiment) {
    $form_state['experiment'] = $experiment;

    foreach ($experiment->entry->link as $link) {
      switch ($link->rel) {
        case 'gwo:testUrl' : $testUrl = $link->href; break;
        case 'gwo:goalUrl' : $goalUrl = $link->href; break;
      }
    }
  }

  $form['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Experiment Title'),
    '#description' => t('The experiment title should be used to uniquely identify this experiment'),
    '#required' => TRUE,
    '#default_value' => isset($experiment) ? (string)$experiment->entry->title->{'$t'} : '',
  );

  $form['testUrl'] = array(
    '#type' => 'textfield',
    '#title' => t('Test Url'),
    '#description' => t('Enter the test page url. It will probably start with @url', array('@url' => $front)),
    '#required' => TRUE,
    '#default_value' => $testUrl,
  );

  $form['goalUrl'] = array(
    '#type' => 'textfield',
    '#title' => t('Conversion Url'),
    '#description' => t('Enter the conversion page url. It will probably start with @urli. This may also be referred to as a Goal URL.', array('@url' => $front)),
    '#required' => TRUE,
    '#default_value' => $goalUrl,
  );

 
  $form['gwo_multivariate_variables'] = array(
    '#type' => 'fieldset',
    '#title' => t('Variables'),
    '#tree' => TRUE,
  );

  $variables = module_invoke_all('gwo_variables');
  foreach ($variables as $module => $variable_settings) {
    $form['gwo_multivariate_variables'][$module] = array(
      '#type' => 'fieldset',
      '#title' => $variable_settings['name'],
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    );

    $form['gwo_multivariate_variables'][$module][$module] = array(
      '#type' => 'checkboxes',
      '#title' => t('Enabled Blocks'),
      '#options' => $variable_settings['options'],
      '#default_value' => isset($test->variable_items[$module]) ? $test->variable_items[$module] : array(),
      '#parents' => array('gwo_multivariate_variables', $module),
    );
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
  /*
  foreach ($form_state['values']['gwo_multivariate_variables'] as $k => $v) {
    $form_state['values']['gwo_multivariate_variables'][$k] = array_filter($v);
  }

  $row = array(
    'header_js'       => $form_state['values']['header_js'],
    'footer_js'       => $form_state['values']['footer_js'],
    'conversion_js'   => $form_state['values']['conversion_js'],
    'test_path'       => $form_state['values']['test_path'],
    'conversion_path' => $form_state['values']['conversion_path'],
    'variable_items'  => serialize($form_state['values']['gwo_multivariate_variables']),
  );

  if ($form_state['values']['sid']) {
    $row['sid'] = $form_state['values']['sid'];
    drupal_write_record('gwo_multivariate', $row, 'sid');
  }
  else {
    drupal_write_record('gwo_multivariate', $row);
  }
  */

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
    $id = (int)$form_state['experiment']->entry->{'gwo$experimentId'}->{'$t'}; 
    $args['url'] = GOOGLE_API_URL ."/{$id}";
    $args['method'] = 'PUT';
    $exp['etag'] = $args['etag'] = $form_state['experiment']->entry->{'gd$etag'};
  }
  $args['data'] = _gwo_multivariate_create_experiment($exp);

  $res = oauth_google_request($args);

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
