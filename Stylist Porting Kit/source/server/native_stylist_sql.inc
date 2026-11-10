// Included by common/sql.cpp immediately after the private Sql definition.
// Only the Stylist batch uses this switch. Restore the core default afterwards.
int32 native_stylist_sql_reconnect(Sql* self,bool enabled){
 if(!self)return SQL_ERROR;my_bool reconnect=enabled?1:0;
 return mysql_options(&self->handle,MYSQL_OPT_RECONNECT,&reconnect)==0?SQL_SUCCESS:SQL_ERROR;
}
