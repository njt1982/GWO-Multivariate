<?php


function gwo_multivariate_admin_overview() {
  $res = oauth_google_request('https://www.google.com/analytics/feeds/websiteoptimizer/experiments', 'GET');
  if ($res->code != 200) {
    drupal_set_message(t('Error getting experiment list from Google. %code: %msg', array('%code' => $res->code, '%msg' => $res->status_message)), 'error');
    return '';
  }

  $obj = simplexml_load_string($res->data);

  $rows = array();
  //while($r = db_fetch_object($result)) {
  foreach ($obj->entry as $experiment) {
    preg_match('/\/([0-9]+)$/', $experiment->id, $id);
    $id = $id[1];

    $rows[] = array(
      check_plain($id),
      check_plain($experiment->title),
      l(t('Edit'), 'admin/settings/gwo-multivariate/edit/'. $id) .' | '. l(t('Delete'), 'admin/settings/gwo-multivariate/delete/'. $id),
    );
  }

  $headers = array(t('Experiment ID'), t('Title'), t('Ops'));

  return theme('table', $headers, $rows);
}


function gwo_multivariate_admin_experiment_form(&$form_state, $test = NULL) {
  $form = array();

  $form['sid'] = array(
    '#type' => 'value',
    '#value' => isset($test->sid) ? $test->sid : NULL,
  );

  $form['test_path'] = array(
    '#type' => 'textfield',
    '#title' => t('Test Path'),
    '#description' => t('Enter the test page source path (eg node/1) - do not use the alias'),
    '#required' => TRUE,
    '#default_value' => isset($test->test_path) ? $test->test_path : '',
    '#field_prefix' => url('<front>', array('absolute' => TRUE)),
  );

  $form['header_js'] = array(
    '#type' => 'textarea',
    '#title' => t('Header JS'),
    '#required' => TRUE,
    '#default_value' => isset($test->header_js) ? $test->header_js : '',
  );

  $form['footer_js'] = array(
    '#type' => 'textarea',
    '#title' => t('Footer JS'),
    '#required' => TRUE,
    '#default_value' => isset($test->footer_js) ? $test->footer_js : '',
  );


  $form['conversion_path'] = array(
    '#type' => 'textfield',
    '#title' => t('Conversion Path'),
    '#description' => t('Enter the conversion page source path (eg node/1) - do not use the alias'),
    '#required' => TRUE,
    '#default_value' => isset($test->conversion_path) ? $test->conversion_path : '',
    '#field_prefix' => url('<front>', array('absolute' => TRUE)),
  );

 
  $form['conversion_js'] = array(
    '#type' => 'textarea',
    '#title' => t('Conversion JS Code'),
    '#required' => TRUE,
    '#default_value' => isset($test->conversion_js) ? $test->conversion_js : '',
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


  $form_state['redirect'] = 'admin/settings/gwo-multivariate';
}


function gwo_multivariate_admin_delete_confirm(&$form_state, $test) {
  $form = array();

  $form['#test'] = $test;
  return confirm_form(
    $form,
    t('Are you sure you want to delete this test?'),
    'admin/settings/gwo-multivariate',
    t('Once deleted, this cannot be retreived'),
    t('Delete'),
    t('Cancel'),
    'gwo_multivariate_admin_delete_confirm'
  );
}

function gwo_multivariate_admin_delete_confirm_submit(&$form, &$form_state) {
  db_query('DELETE FROM {gwo_multivariate} WHERE sid=%d', $form['#test']->sid);
  $form_state['redirect'] = 'admin/settings/gwo-multivariate';
}



/*
function gwo_multivariate_admin_oauth() {
  $form = array();

  $form['gwo_oauth_consumer_key'] = array(
    '#type' => 'textfield',
    '#title' => t('Consumer Key'),
    '#description' => t('You can get a consumer key from !link', array('!link' => l(t('Google Manage Domains'), 'https://www.google.com/accounts/ManageDomains', array('absolute' => TRUE)))),
    '#required' => TRUE,
    '#default_value' => variable_get('gwo_oauth_consumer_key', ''),
  );

  $form['gwo_oauth_consumer_secret'] = array(
    '#type' => 'textfield',
    '#title' => t('Consumer Secret'),
    '#description' => t('You can get a consumer secret from !link', array('!link' => l(t('Google Manage Domains'), 'https://www.google.com/accounts/ManageDomains', array('absolute' => TRUE)))),
    '#required' => TRUE,
    '#default_value' => variable_get('gwo_oauth_consumer_secret', ''),
  );


  //$form['submit'] = array('#type' => 'submit', '#value' => t('Save Settings'));
  return system_settings_form($form);
}
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
